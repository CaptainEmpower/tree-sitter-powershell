// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See the LICENSE file in the project root for full license information.

#include "tree_sitter/parser.h"
#include <wctype.h>

enum TOKEN_TYPE {
    STATEMENT_TERMINATOR,
    BARE_DASHES
};

/* --- API --- */

void *tree_sitter_powershell_external_scanner_create();

void tree_sitter_powershell_external_scanner_destroy(void *p);

unsigned tree_sitter_powershell_external_scanner_serialize(void *payload, char *buffer);

void tree_sitter_powershell_external_scanner_deserialize(void *payload, const char *buffer, unsigned length);

bool tree_sitter_powershell_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols);

/* --- Internal Functions --- */

static void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

// semcode patch (terminator end): the caller marks the token's end, where
// the scan started, before any blank either scan skips.
static bool scan_statement_terminator(void *payload, TSLexer *lexer, const bool *valid_symbols)
{
    if (valid_symbols[STATEMENT_TERMINATOR]) {
        lexer->result_symbol = STATEMENT_TERMINATOR;
        // This token has no characters -- everything is lookahead to determine its existence

        for (;;) {
            if (lexer->lookahead == 0) return true;
            if (lexer->lookahead == '}') return true;
            if (lexer->lookahead == ';') return true;
            if (lexer->lookahead == ')') return true;
            if (lexer->lookahead == '\n') return true;
            // semcode patch (#258): PowerShell ends a line at a lone CR too
            if (lexer->lookahead == '\r') return true;
            if (!iswspace(lexer->lookahead)) return false;
            skip(lexer);
        }
    }

    return false;
}

// A bare `-` or `--` argument: `--` ends option parsing for a native command
// (`git log -- path`, `cargo test -- --nocapture`) and `-` names stdin
// (`tar -xf -`). `--` is also the decrement operator and `-` the unary minus,
// so the lexer cannot tell them apart by length or precedence: the token is
// recognised only when the dashes are followed by whitespace or the end of the
// command.
// semcode patch (terminator end): called with the lookahead on the first
// dash, after the caller skipped the blanks before it.
static bool scan_bare_dashes(void *payload, TSLexer *lexer, const bool *valid_symbols)
{
    lexer->advance(lexer, false);
    if (lexer->lookahead == '-') lexer->advance(lexer, false);
    lexer->mark_end(lexer);

    switch (lexer->lookahead) {
        case 0:
        case ' ':
        case '\t':
        // semcode patch (terminator end): PowerShell's other blanks
        case '\f':
        case '\v':
        case '\r':
        case '\n':
        case ';':
        case '|':
        case '&':
        case ')':
        case '}':
            lexer->result_symbol = BARE_DASHES;
            return true;
        default:
            return false;
    }
}

/* --- API Implementation --- */

bool tree_sitter_powershell_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols)
{
    // semcode patch (terminator end): the zero-width terminator ends where the
    // scan starts, so it is marked before the bare-dash scan skips any blank;
    // otherwise a statement would end after the blanks that follow it. A
    // successful bare-dash scan marks its own end past the dashes.
    if (valid_symbols[STATEMENT_TERMINATOR]) lexer->mark_end(lexer);

    // The statement terminator consumes nothing, so it must be tried last: when
    // both are valid, a `--` has to win over the empty terminator.
    if (valid_symbols[BARE_DASHES]) {
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t') skip(lexer);
        // A dash is no blank, so no terminator follows here either: a dash
        // that is not a bare-dash argument is left to the parser's lexer,
        // never dropped behind a terminator.
        if (lexer->lookahead == '-') return scan_bare_dashes(payload, lexer, valid_symbols);
    }
    return scan_statement_terminator(payload, lexer, valid_symbols);
}

void *tree_sitter_powershell_external_scanner_create()
{
    return NULL;
}

void tree_sitter_powershell_external_scanner_destroy(void *p)
{
}

unsigned tree_sitter_powershell_external_scanner_serialize(void *payload, char *buffer)
{
    return 0;
}

void tree_sitter_powershell_external_scanner_deserialize(void *payload, const char *buffer, unsigned length)
{
}
