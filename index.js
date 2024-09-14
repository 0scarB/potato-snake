window.onload = () => {
    let wasm_mem;

    const WASM_MEM_PAGE_SIZE = 64*1024;
    let wasm_dyn_mem_start = -1;
    let wasm_dyn_mem_ptr   = -1;
    let wasm_dyn_mem_stop  = -1;

    const terminal_width  = 2*30;
    const terminal_height = 50;
    let blank_terminal_str = "";
    for (let y = 0; y < terminal_height; ++y) {
        for (let x = 0; x < terminal_width; ++x) {
            blank_terminal_str += " ";
        }
        blank_terminal_str += "\n";
    }
    let terminal_str = blank_terminal_str;
    let terminal_cursor_x = 0;
    let terminal_cursor_y = 0;
    const terminal_str_idx = () =>
        terminal_cursor_y*(terminal_width + 1) + terminal_cursor_x;

    const utf8_text_decoder = new TextDecoder();

    const terminal_el = document.getElementById("terminal");

    const UP    = 0;
    const RIGHT = 1;
    const DOWN  = 2;
    const LEFT  = 3;
    const QUIT  = 4;
    let input = -1;
    window.addEventListener("keydown", (event) => {
        switch (event.key) {
            case "w": input = UP   ; break;
            case "a": input = LEFT ; break;
            case "s": input = DOWN ; break;
            case "d": input = RIGHT; break;
            case "q": input = QUIT ; break;
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
            wasm_terminal_write(str_ptr, str_len) {
                const str = utf8_text_decoder.decode(
                    new Uint8Array(wasm_mem.buffer, str_ptr, str_len));

                const old_terminal_str = terminal_str;
                terminal_str = terminal_str.slice(0, terminal_str_idx());
                terminal_str += str;
                terminal_cursor_x += str.length;
                terminal_str += old_terminal_str.slice(terminal_str_idx());
            },
            wasm_terminal_write_int(x) {
                const str = parseInt(x);

                const old_terminal_str = terminal_str;
                terminal_str = terminal_str.slice(0, terminal_str_idx());
                terminal_str += str;
                terminal_cursor_x += str.length;
                terminal_str += old_terminal_str.slice(terminal_str_idx());
            },
            wasm_terminal_move_cursor(x, y) {
                terminal_cursor_x = x;
                terminal_cursor_y = y;
            },
            wasm_terminal_flush_out() {
                terminal_el.innerHTML = terminal_str;
            },
        }},
    ).then(result => {
        const wasm_inst = result.instance;
        wasm_mem = wasm_inst.exports.memory;

        wasm_dyn_mem_start = wasm_mem.grow(1)*WASM_MEM_PAGE_SIZE;
        wasm_dyn_mem_ptr   = wasm_dyn_mem_start;
        wasm_dyn_mem_stop  = wasm_dyn_mem_start*WASM_MEM_PAGE_SIZE;

        wasm_inst.exports.update();
        setInterval(wasm_inst.exports.update, 100);
        wasm_inst.exports.update();
    });
}
