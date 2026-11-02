# semcode patches to tree-sitter-powershell

This is [airbus-cert/tree-sitter-powershell] 0.26.6 (upstream tag `v0.26.6`,
commit `65ea691`, MIT, see `LICENSE`), vendored into semcode with the changes
below. semcode first vendored 0.26.4 (`d398441`); its patches were rebased
onto 0.26.6 unchanged, and two more (grammar change 5, the "terminator end"
scanner change) fix code that 0.26.6 added. Each one is meant to go upstream; once a
release carries them all, drop this directory and point the workspace
dependency back at crates.io. Why semcode vendors it: ADR-163 (`docs/adrs/ADR-163-powershell-grammar.md`).

Upstream files are kept whole, not split to semcode's 300-line rule, so
that they stay diffable against upstream; `src/` is generated and is never
edited by hand, except the hand-written external scanner `src/scanner.c`
(see "Scanner change" below).

The crate keeps upstream's package and library names (`tree-sitter-powershell`,
`tree_sitter_powershell`), so `tree-sitter-language-adapter` uses it unchanged.
Its version, `0.26.6+semcode.N`, carries the patch level as semver build
metadata; bump `N` (here and in `PowerShellAdapter::get_version`) with each
patch. `publish = false`: it reaches downstream only as a path dependency of
the semcode workspace.

## Regenerating

`src/parser.c`, `src/grammar.json`, `src/node-types.json` and
`src/tree_sitter/*.h` are generated from `grammar.js`. From the repository root:

- `just powershell-grammar-generate`: runs `tree-sitter generate` with
  tree-sitter-cli 0.25.10, the version of the `tree-sitter` crate semcode
  links, so the parser is ABI 15. Upstream generated 0.26.6 with CLI 0.26.x;
  regenerating the unpatched grammar with 0.25.10 changes only the
  generator banner in `parser.c` and the bundled `array.h` (as for 0.26.4).
- `just powershell-grammar-test`: upstream's corpus in `test/corpus/` plus
  `test/corpus/semcode_patches.txt`, one case per patch below.
- `just powershell-grammar-check`: both, then fails if the committed output
  differs from what `grammar.js` generates.

`tree-sitter generate` reports no conflicts and no warnings for the patched
grammar (2195 parse states; upstream's 0.26.6 parser has 2192), and the
`conflicts` list is upstream's own, unchanged.

## Grammar changes (`grammar.js`)

Each changed rule carries a `(semcode patch, #N)` comment, or for change 5,
which has no issue, `(semcode patch: attached values stay literal)`. Issue
numbers are CaptainEmpower/semcode issues, sub-issues of #253 (Ansible
Windows modules that fail to parse).

1. **#254, generic type literals.**
   - `type_identifier` accepts a backtick arity suffix:
     ``[System.Collections.Generic.List`1[String]]``.
   - `generic_type_arguments` accepts each argument bare or wrapped in
     brackets, through a new hidden `_generic_type_argument`:
     `[Dictionary[[String], [Object]]]`, `[Func[[Object], [UInt32]]]`. The
     argument is still a `type_spec` child, so the tree shape for bare
     arguments is unchanged.
2. **#256, nested type names.** `type_name` accepts `+` as a separator, like
   `.`: `[System.Net.WebRequestMethods+Ftp]`, `[Environment+SpecialFolder]`.
3. **#257, numeric literals in any case.** PowerShell reads numeric suffixes
   case-insensitively; upstream spelled them in lower case only, so `1GB`
   lexed as a command name (`$a = 1KB` "parsed" as running a command `1KB`,
   and `$s = $n * 1GB` failed). Shared constants `NUMERIC_LONG_SUFFIX`
   (`l`/`L`), `NUMERIC_DECIMAL_SUFFIX` (`d`/`D`), `NUMERIC_MULTIPLIER`
   (`kb`…`pb`, any case) and `NUMERIC_EXPONENT` (`e`/`E`) now build
   `decimal_integer_literal`, `hexadecimal_integer_literal` (prefix `0x` or
   `0X`) and `real_literal`. `real_literal` also gains the optional `d`
   suffix the language spec gives it (`1.5d`, spec 2.3.5.1.2).
4. **#258, bare label names.** `label_expression` accepts a `simple_name`, so
   `break regLoop` / `continue regLoop` parse; `break :regLoop` and
   `break $label` are unchanged.

5. **Attached values stay literal** (found in review of semcode#261; no
   issue of its own). Upstream 0.26.6 (`b03e6b0`) added a `command_parameter`
   alternative for a native option with an attached value (`--format=%H`)
   whose value took `$x`, `${x}`, `$(...)`, backticks and any double-quoted
   text. PowerShell expands all of these in `--x=...` (a generic token), and
   in `-x=...` once a quote appears (the tokenizer rescans such a parameter
   as a generic argument, `ScanParameter`). So `iex -Command="$(Get-Secret)"`
   or `powershell.exe --EncodedCommand=$payload` parsed cleanly with the
   code inside one opaque leaf, where before 0.26.6 they did not parse at
   all. Now the value takes only text that cannot expand: no `$`, no
   backtick, single-quoted parts, and double-quoted parts with neither `$`
   nor a backtick. A second alternative keeps PowerShell's literal
   parameter token `-x=$y` (one dash, no quote, `$` included). An expanding
   form lexes as the name and `=` with the rest left over, and today all of
   them parse with an error, so they are reported as unparsed.
   `--format=%H`, `--pretty='format:%B'` and `--pretty=format:"%h %s"`
   are unchanged. Tested by `test/corpus/semcode_patches.txt` and by
   `attached_value_tests.rs` in
   `tree-sitter-language-adapter`.

## Scanner changes (`src/scanner.c`)

`src/scanner.c` is hand-written upstream (tree-sitter generate does not
touch it).

- **A lone CR ends a statement.** `scan_statement_terminator` returned at
  `\n` but skipped `\r` as whitespace, while PowerShell ends a line at a
  lone CR too. With #258, a zero-argument command on the line after
  `break` in a CR-only file (`break\riex`) parsed cleanly as the label
  instead of a command. Returning at `\r` makes it a statement of its own.
  CRLF files are unchanged: the token is zero-width, so ending at the CR
  rather than at the LF that follows it gives the same tree. Tested by
  `a_lone_carriage_return_ends_the_statement_after_break` in
  `tree-sitter-language-adapter`.
- **The terminator ends where the scan starts** (found in review of
  semcode#261). Upstream 0.26.6's `scan_bare_dashes` skipped spaces and
  tabs and, failing, let `scan_statement_terminator` mark the zero-width
  terminator after them. As a result, a statement in a one-line block took
  in the blank before `}` (`if ($a) { Get-X b }`: the inner `statement_list`
  ran 10..18, not 10..17), which changed the extents in 42 of the 289
  Ansible modules that parsed before 0.26.6. A failed bare-dash scan also
  left the lexer past the dash, so with `\f` or `\v` and then a line end
  the terminator dropped the dash from the tree (`iex $x -\f\n`). Now the
  scan entry point marks the terminator's end before skipping anything. It
  tries the bare-dash scan only when the next non-blank character is a dash;
  a dash is no blank, so if that scan fails no terminator follows and the
  dash is left to the parser's lexer. `\f` and `\v`, which PowerShell reads
  as blanks, end a bare dash like a space does. Tested by
  `statement_end_tests.rs` in `tree-sitter-language-adapter`.

## Binding changes (`bindings/rust/`)

These are semcode build hygiene, not grammar changes; upstream's own
template would need them too.

- `lib.rs`: the doc example parses `Write-Output "hello"` instead of an
  empty script. The grammar rejects an empty script (ADR-163,
  Consequences), so upstream's example failed as a doctest.
- `build.rs`: the per-query `cfg` checks are a loop over the query files,
  instead of the template's `!"…".is_empty() && …` (clippy `const_is_empty`
  under `-D warnings`), and the C build passes `-Wno-unused-parameter`
  (the stateless external scanner leaves its hook parameters unused).
- `Cargo.toml`: version, `publish = false`, the `tree-sitter` dev-dependency
  taken from the workspace (0.25) instead of 0.26.5. The crate does not opt
  into `[workspace.lints]`: its binding needs `unsafe` for the C entry point,
  which the workspace forbids.

## Fixed upstream since 0.26.4

- #255, a comma list of bare words as a command argument
  (`Import-Module -Name A, B`), is fixed by upstream `b03e6b0` ("Parse
  native-command arguments: bare -- and -, --opt=value, -A5, comma lists,
  trailing &"), released in 0.26.6. The words are `generic_token` children
  of `command_elements`, joined by `,` tokens with no
  `command_argument_sep` between them; the grammar gives the list no node
  of its own. The same commit's bare-dash scanner token (`_bare_dashes`,
  which yields a `command_parameter`) parses a lone `-` argument
  (`-failifempty -`), one of #259's constructs. The tests are in
  `tree-sitter-language-adapter`'s `powershell/native_argument_tests.rs`.

## Not fixed here

- #259's remaining constructs: an index on a method call's result as a
  command argument (`Compare-XmlDoc $a.get_ChildNodes()[$i] ...`), an
  assembly-qualified type literal (`[T, Assembly, ContentType = ...]`), and
  a switch clause whose string starts with `^(` (`switch -Regex { "^\[(.+)\]"
  { ... } }`).

[airbus-cert/tree-sitter-powershell]: https://github.com/airbus-cert/tree-sitter-powershell
