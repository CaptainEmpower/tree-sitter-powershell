# CaptainEmpower fork of tree-sitter-powershell (PATCHES.md).
# The CLI is pinned to the tree-sitter crate's version semcode links, so the
# generated parser is ABI 15, loadable by `tree-sitter` 0.25. Needs Node.js
# (npx) and a C compiler (`tree-sitter test` compiles the parser).
ts_cli := "npx --yes tree-sitter-cli@0.25.10"

# Regenerate src/parser.c, grammar.json and node-types.json from grammar.js.
# Fails on any unresolved conflict; commit grammar.js and the output together.
generate:
    {{ts_cli}} generate

# Run the corpus (upstream's plus test/corpus/semcode_patches.txt).
test:
    {{ts_cli}} test

# Fail if the committed parser is not what grammar.js generates. The Grammar
# workflow (.github/workflows/grammar.yml) runs the same steps; keep the two
# in sync.
check: generate test
    git diff --exit-code -- src

# Upstream's eslint configuration over grammar.js.
lint:
    npm ci --legacy-peer-deps
    npm run lint

# The Rust binding: builds the C parser and runs the doctest.
cargo-test:
    cargo test --locked
