fn main() {
    let src_dir = std::path::Path::new("src");

    let mut c_config = cc::Build::new();
    c_config.std("c11").include(src_dir);
    // The external scanner is stateless, so its create/destroy/serialize
    // hooks leave their parameters unused by design; cc's -Wextra would
    // print one warning per parameter on every build. (semcode)
    c_config.flag_if_supported("-Wno-unused-parameter");

    #[cfg(target_env = "msvc")]
    c_config.flag("-utf-8");

    if std::env::var("TARGET").unwrap() == "wasm32-unknown-unknown" {
        let Ok(wasm_headers) = std::env::var("DEP_TREE_SITTER_LANGUAGE_WASM_HEADERS") else {
            panic!("Environment variable DEP_TREE_SITTER_LANGUAGE_WASM_HEADERS must be set by the language crate");
        };
        let Ok(wasm_src) =
            std::env::var("DEP_TREE_SITTER_LANGUAGE_WASM_SRC").map(std::path::PathBuf::from)
        else {
            panic!("Environment variable DEP_TREE_SITTER_LANGUAGE_WASM_SRC must be set by the language crate");
        };

        c_config.include(&wasm_headers);
        c_config.files([
            wasm_src.join("stdio.c"),
            wasm_src.join("stdlib.c"),
            wasm_src.join("string.c"),
        ]);
    }

    let parser_path = src_dir.join("parser.c");
    c_config.file(&parser_path);
    println!("cargo:rerun-if-changed={}", parser_path.to_str().unwrap());

    let scanner_path = src_dir.join("scanner.c");
    if scanner_path.exists() {
        c_config.file(&scanner_path);
        println!("cargo:rerun-if-changed={}", scanner_path.to_str().unwrap());
    }

    c_config.compile("tree-sitter-powershell");

    // One cfg per query file the crate ships, so lib.rs exports only those.
    // (semcode: the generator's template tested each literal path with
    // `!"…".is_empty()`, which clippy's `const_is_empty` rejects.)
    for (cfg, query) in [
        ("with_highlights_query", "queries/highlights.scm"),
        ("with_injections_query", "queries/injections.scm"),
        ("with_locals_query", "queries/locals.scm"),
        ("with_tags_query", "queries/tags.scm"),
    ] {
        println!("cargo:rustc-check-cfg=cfg({cfg})");
        if std::path::Path::new(query).exists() {
            println!("cargo:rustc-cfg={cfg}");
        }
    }
}
