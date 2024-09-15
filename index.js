window.onload = () => {
    let wasm_mem;

    const WASM_MEM_PAGE_SIZE = 64*1024;
    let wasm_dyn_mem_start = -1;
    let wasm_dyn_mem_ptr   = -1;
    let wasm_dyn_mem_stop  = -1;

    const terminal_el            = document.getElementById("terminal");
    const text_size_reference_el = document.getElementById("text_size_reference");
    const monospace_text_advance_x = text_size_reference_el.clientWidth /4;
    const monospace_text_advance_y = text_size_reference_el.clientHeight/4;
    const terminal_width  = Math.floor(terminal_el.clientWidth /monospace_text_advance_x);
    const terminal_height = Math.floor(terminal_el.clientHeight/monospace_text_advance_y);

    const line_len = terminal_width + 1;
    const target_line_group_len = 1024;
    const lines_per_line_group = Math.ceil(target_line_group_len/line_len);
    const blank_line            = ' '.repeat(terminal_width) + '\n';
    const blank_line_group_text = blank_line.repeat(lines_per_line_group);
    const line_groups_texts      = [];
    const line_groups_text_nodes = [];
    for (let line = 0; line < terminal_height; line += lines_per_line_group) {
        const line_group_text = blank_line_group_text;

        line_groups_texts.push(line_group_text);

        const line_group_el = document.createElement("span");
        line_group_el.style.position = "absolute";
        line_group_el.style.display  = "block";
        line_group_el.style.top      = line*monospace_text_advance_y + "px";
        line_group_el.style.left     = "0";
        line_group_el.innerHTML      = line_group_text;
        terminal_el.appendChild(line_group_el);

        line_groups_text_nodes.push(line_group_el.childNodes[0]);
    }
    const line_groups_need_update_idxs = [];

    let terminal_cursor_x = 0;
    let terminal_cursor_y = 0;

    const terminal_write = (str) => {
        const line_offset    =  terminal_cursor_y % lines_per_line_group;
        const line_group_idx = (terminal_cursor_y - line_offset)/lines_per_line_group;
        const start_idx      = line_offset*line_len + terminal_cursor_x;

        const old_text = line_groups_texts[line_group_idx];
        line_groups_texts[line_group_idx] =
            old_text.slice(0, start_idx) + str + old_text.slice(start_idx + str.length);

        if (line_groups_need_update_idxs.indexOf(line_group_idx) == -1) {
            line_groups_need_update_idxs.push(line_group_idx);
        }

        terminal_cursor_x += str.length;
    }

    const utf8_text_decoder = new TextDecoder();

    const UP     = 0;
    const RIGHT  = 1;
    const DOWN   = 2;
    const LEFT   = 3;
    const REPLAY = 4;
    const QUIT   = 5;
    let input = -1;
    window.addEventListener("keydown", (event) => {
        switch (event.key) {
            case "w": input = UP    ; break;
            case "a": input = LEFT  ; break;
            case "s": input = DOWN  ; break;
            case "d": input = RIGHT ; break;
            case "r": input = REPLAY; break;
            case "q": input = QUIT  ; break;
        }
    });

    WebAssembly.instantiateStreaming(
        fetch("./snake.wasm", {headers: {"If-Modified-Since": ""}}),
        {env: {
            rand() {
                return Math.random()*(1<<24);
            },
            sbrk(size) {
                while (size > wasm_dyn_mem_stop - wasm_dyn_mem_ptr) {
                    wasm_mem.grow(1);
                    wasm_dyn_mem_stop += WASM_MEM_PAGE_SIZE;
                }

                const result = wasm_dyn_mem_ptr;

                wasm_dyn_mem_ptr += size;

                return result;
            },
            brk(ptr) {
                wasm_dyn_mem_ptr = ptr;
                return 0;
            },
            wasm_capture_input() {
                result = input;
                input = -1;
                return result;
            },
            wasm_get_terminal_dims() {
                const  packed_dims = terminal_width<<16 | terminal_height;
                return packed_dims;
            },
            wasm_terminal_write(str_ptr, str_len) {
                const str = utf8_text_decoder.decode(
                    new Uint8Array(wasm_mem.buffer, str_ptr, str_len));
                terminal_write(str);
            },
            wasm_terminal_write_int(x) {
                terminal_write(x.toString());
            },
            wasm_terminal_move_cursor(x, y) {
                terminal_cursor_x = x;
                terminal_cursor_y = y;
            },
            wasm_terminal_flush_out() {
                for (const idx of line_groups_need_update_idxs) {
                    line_groups_text_nodes[idx].nodeValue = line_groups_texts[idx];
                }
                line_groups_need_update_idxs.length = 0;
            },
            wasm_terminal_clear() {
                for (let idx = 0; idx < line_groups_texts.length; ++idx) {
                    line_groups_texts[idx] = blank_line_group_text;

                    if (line_groups_need_update_idxs.indexOf(idx) == -1) {
                        line_groups_need_update_idxs.push(idx);
                    }
                }
            }
        }},
    ).then(result => {
        const wasm_inst = result.instance;
        wasm_mem = wasm_inst.exports.memory;

        wasm_dyn_mem_start = wasm_mem.grow(1)*WASM_MEM_PAGE_SIZE;
        wasm_dyn_mem_ptr   = wasm_dyn_mem_start;
        wasm_dyn_mem_stop  = wasm_dyn_mem_start;

        const update = wasm_inst.exports.update;
        let t0 = performance.now()/1000;
        let update_interval = update();
        const mainLoop = () => {
            const t1 = performance.now()/1000;
            if (t1 - t0 > update_interval) {
                t0 = t1;
                update_interval = update();
            }
            requestAnimationFrame(mainLoop);
        }
        requestAnimationFrame(mainLoop);
    });
}

