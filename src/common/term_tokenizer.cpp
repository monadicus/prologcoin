#include "term_tokenizer.hpp"

//
// Splitting up characters into tokens is more complicated than
// you think. I used the documentation of
// https://sicstus.sics.se/sicstus/docs/3.7.1/html/sicstus_45.html
// to guide myself into doing a proper Prolog tokenization.
//
//
// Syntax of Tokens as Character Strings
//
// layout-char  In ISO 8859/1, these are character codes 0..32 and 127..159.
//              In EUC, these are character codes 0..32 and 127. The common
//              subset includes characters such as TAB, LFD, and SPC.
//
// small-letter In ISO 8859/1, these are character codes 97..122, 223..246, 
//              and 248..255. In EUC, these are character codes 97..122 and 
//              128..255. The common subset includes the letters a through z.
//
// capital-letter   In ISO 8859/1, these are character codes 65..90, 192..214,
//                  and 216..222. In EUC, these are character codes 65..90.
//                  The common subset is the letters A through Z.
//
// digit        In both standards, these are character codes 48..57, i.e. the 
//              digits 0 through 9.
//
// symbol-char  In ISO 8859/1, these are character codes 35, 36, 38, 42, 43,
//              45..47, 58, 60..64, 92, 94, 96, 126, 160..191, 215, and 247.
//              In EUC, these are character codes 35, 36, 38, 42, 43, 45..47,
//              58, 60..64, 92, 94, 96, and 126. The common subset isa
//              +-*/\^<>=`~:.?@#$&.
//
// solo-char    In both standards, these are character codes 33 and 59 i.e.
//              the characters ! and ;.
//
// punctuation-char    In both standards, these are character codes 37, 40, 41,
//                     44, 91, 93, and 123..125, i.e. the characters %(),[]{|}.
//
// quote-char   In both standards, these are character codes 34 and 39 i.e.
//              the characters " and '.
//
// underline    In both standards, this is character code 95 i.e.
//              the character _.
//
// token            --> name
//                   |  natural-number
//                   |  unsigned-float
//                   |  variable
//                   |  string
//                   |  punctuation-char
//                   |  layout-text
//                   |  full-stop
//
// name              --> quoted-name
//                    |  word
//                    |  symbol
//                    |  solo-char
//                    |  [ ?layout-text ]
//                    |  { ?layout-text }
//
// quoted-name       --> ' ?quoted-item... '
// 
// quoted-item       --> char  { other than ' or \ }
//                    |  ''
//                    |  \ escape-sequence
// 
// word              --> small-letter ?alpha...
//
// symbol            --> symbol-char...
//                          { except in the case of a full-stop
//                            or where the first 2 chars are /* }
//
// natural-number    --> digit...
//                    |  base ' alpha...
//                          { where each alpha must be less than the base,
//                          treating a,b,... and A,B,... as 10,11,... }
//                    |  0 ' char-item
//                          { yielding the character code for char }
//
// char-item         --> char  { other than \ }
//                    |  \ escape-sequence
//  
// base              --> digit...  { in the range [2..36] }
//
// unsigned-float    --> simple-float
//                    |  simple-float exp exponent
// 
// simple-float      --> digit... . digit...
//
// exp               --> e  |  E
//
// exponent          --> digit... | sign digit...
//
// sign              --> - | +
//
// variable          --> underline ?alpha...
//                    |  capital-letter ?alpha...
//
// string            --> " ?string-item... "
//
// string-item       --> char  { other than " or \ }
//                    |  ""
//                    |  \ escape-sequence
//
// layout-text             --> layout-text-item...
//
// layout-text-item        --> layout-char | comment
//
// comment           --> /* ?char... */
//                       { where ?char... must not contain */ }
//                    |  % ?char... LFD
//                       { where ?char... must not contain LFD }
//
// full-stop         --> .
//                       { the following token, if any, must be layout-text}
//
// char              --> { any character, i.e. }
//                      layout-char
//                   |  alpha
//                   |  symbol-char
//                   |  solo-char
//                   |  punctuation-char
//                   |  quote-char
//
// alpha             --> capital-letter | small-letter | digit | underline
//
// escape-sequence   --> b        { backspace, character code 8 }
//                    |  t        { horizontal tab, character code 9 }
//                    |  n        { newline, character code 10 }
//                    |  v        { vertical tab, character code 11 }
//                    |  f        { form feed, character code 12 }
//                    |  r        { carriage return, character code 13 }
//                    |  e        { escape, character code 27 }
//                    |  d        { delete, character code 127 }
//                    |  a        { alarm, character code 7 }
//                    |  x alpha alpha
//                                {treating a,b,... and A,B,... as 10,11,... }
//                                { in the range [0..15], hex character code }
//                    |  digit ?digit ?digit 
//                                { in the range [0..7], octal character code }
//                    |  ^ ?      { delete, character code 127 }
//                    |  ^ capital-letter
//                    |  ^ small-letter
//                                { the control character alpha mod 32 }
//                    |  c ?layout-char... { ignored }
 

namespace prologcoin { namespace common {

term_tokenizer::term_tokenizer(std::istream &in, term_ops &ops)
        : in_(in),
	  ops_(ops),
          column_(0),
          line_(0)
{
    (void)in_;
    (void)ops_;
    (void)column_;
    (void)line_;
}


void term_tokenizer::next_quoted_name()
{
    int ch = next_char();
    assert(ch == '\'');

    current_.type_ = TOKEN_NAME;

    while (!is_eof()) {
	ch = next_char();
	if (ch == '\'') {
	    // Check if next is also a '
	    if (peek_char() == '\'') {
		current_.lexeme_ += '\'';
	    } else {
		return;
	    }
	} else if (ch != '\\') {
	    current_.lexeme_ += static_cast<char>(ch);
	} else {
	    next_escape_sequence();
	}
    }
}

void term_tokenizer::next_escape_sequence()
{
    int ch = next_char();

    char c;

    switch (ch) {
    case 'b': c = '\b'; break;
    case 't': c = '\t'; break;
    case 'n': c = '\n'; break;
    case 'v': c = 11; break;
    case 'f': c = 12; break;
    case 'r': c = '\r'; break;
    case 'e': c = 27; break;
    case 'd': c = 127; break;
    case 'a': c = 7; break;
    case 'x':{
	int v1 = parse_hex_digit();
	int v2 = parse_hex_digit();
	c = (v1 << 8) | v2;
	break;
        }
    case 'c': {
	while (is_layout_char(peek_char())) {
	    ch = next_char();
	}
	return;
        }
    case '^': {
	ch = next_char();
	if (ch == '?') {
	    c = 127; break;
	} else if (is_capital_letter(ch)) {
	    c = (ch - 'A' + 1) % 32;
	} else if (is_small_letter(ch)) {
	    c = (ch - 'a' + 1) % 32;
	} else {
	    throw token_exception_control_char(
		       "Unexpected control character (" 
				+ boost::lexical_cast<std::string>(c) + ")");
	}
	break;
        }
    default:
	{
	    if (ch >= '0' && ch <= '7') {
		c = static_cast<char>(parse_oct_code());
	    } else if (is_layout_char(ch)) {
		return;
	    } else {
		c = static_cast<char>(ch);
	    }
	}
	break;
    }
    current_.lexeme_ += c;
}

int term_tokenizer::parse_hex_digit()
{
    int ch = next_char();
    int v = 0;
    if (ch >= 'a' && ch <= 'f') {
	v = ch - 'a' + 10;
    } else if (ch >= 'A' && ch <= 'F') {
	v = ch - 'A' + 10;
    } else if (ch >= '0' && ch <= '9') {
	v = ch - '0';
    } else {
	throw token_exception_hex_code(
	       "Unexpected hex character ("
	       + boost::lexical_cast<std::string>(ch) + ")");
    }
    return v;
}

int term_tokenizer::parse_oct_code()
{
    int v = 0;
    int ch;

    for (int i = 0; i < 3; i++) {
	ch = next_char();
	if (ch >= '0' && ch <= '7') {
	    v = v << 3;
	    v |= (ch - '0');
	} else {
	    break;
	}
    }
    return v;
}

void term_tokenizer::next_word()
{
    next_alphas();
}

void term_tokenizer::next_variable()
{
    next_alphas();
}

void term_tokenizer::next_layout_text()
{
    bool cont = true;

    while (cont) {
        int ch = peek_char();
	if (is_layout_char(ch)) {
	    consume_next_char();
	} else if (ch == '/' && is_comment_begin()) {
	    parse_block_comment();
	} else if (ch == '%') {
	    parse_line_comment();
	} else {
	    cont = false;
	}
    }
}

void term_tokenizer::next_solo_char()
{
    consume_next_char();
}

void term_tokenizer::next_punctuation_char()
{
    consume_next_char();
}

void term_tokenizer::next_full_stop()
{
    consume_next_char();
}

void term_tokenizer::parse_block_comment()
{
    int depth = 0;
    int ch = next_char_la();
    if (ch == '/' && peek_char() == '*') {
	unget_char();
	consume_next_char();
	consume_next_char();
	depth++;
    } else {
	// This wasn't a block comment!
	unget_char();
	return;
    }

    while (depth > 0 && !is_eof()) {
	int ch = next_char();
	if (ch == '*' && peek_char() == '/') {
	    (void)next_char();
	    depth--;
	    current_.lexeme_ += "*/";
	} else if (ch == '/' && peek_char() == '*') {
	    (void)next_char();
	    depth++;
	    current_.lexeme_ += "/*";
	} else {
	    current_.lexeme_ += static_cast<char>(ch);
	}
    }
}

void term_tokenizer::parse_line_comment()
{
    if (peek_char() != '%') {
	// This wasn't a line comment!
	return;
    }

    while (peek_char() != '\n') {
	if (is_eof()) {
	    return;
	}
	consume_next_char();
    }
    consume_next_char();
}

void term_tokenizer::next_symbol()
{
    while (is_symbol_char(peek_char()) && !is_comment_begin()
	   && !is_full_stop()) {
	consume_next_char();
    }
}

bool term_tokenizer::is_comment_begin() const
{
    if (peek_char() == '%') {
	return true;
    }
    if (peek_char() != '/') {
	return false;
    }
    next_char_la();
    if (peek_char() == '*') {
	unget_char();
	return true;
    }
    unget_char();
    return false;
}

bool term_tokenizer::is_full_stop() const
{
    return peek_char() == '.';
}

bool term_tokenizer::is_full_stop(int ch) const
{
    return ch == '.';
}

void term_tokenizer::next_xs( const std::function<bool(int)> &predicate )
{
    while (predicate(peek_char())) {
	consume_next_char();
    }
}

void term_tokenizer::next_digits()
{
    return next_xs( [](int ch) { return is_digit(ch); } );
}

void term_tokenizer::next_alphas()
{
    return next_xs( is_alpha );
}

void term_tokenizer::next_char_code()
{
    if (peek_char() == '\\') {
	(void) next_char();
	current_.reset();
	next_escape_sequence();
    } else {
	current_.reset();
	consume_next_char();
    }
    // Decode it.
    if (current_.lexeme_.empty()) {
	throw token_exception_no_char_code(
					   "No char code provided for 0'");
	
    }
    int c = (char)current_.lexeme_[0];
    current_.lexeme_ = boost::lexical_cast<std::string>(c);
}

void term_tokenizer::next_number()
{
    next_digits();

    int ch = peek_char();

    if (ch == '\'') {
	consume_next_char();

	if (current_.lexeme_ == "0'") {
	    next_char_code();
	} else {
	    next_alphas();
	}
    } else if (ch == '.' || ch == 'e' || ch == 'E') {
	set_token_type(TOKEN_UNSIGNED_FLOAT);
	next_float();
    }
}

void term_tokenizer::next_float()
{
    int ch = peek_char();
    if (ch == 'e' || ch == 'E') {
	next_exponent();
    } else if (ch == '.') {
	next_decimal();
    }
}

void term_tokenizer::next_decimal()
{
    consume_next_char();
    next_digits();
    int ch = peek_char();
    if (ch == 'e' || ch == 'E') {
	next_exponent();
    }
}

void term_tokenizer::next_exponent()
{
    consume_next_char();
    int ch = peek_char();
    if (ch == '+' || ch == '-') {
	consume_next_char();
    }
    next_digits();
}

void term_tokenizer::next_string()
{
    next_char(); // Skip "

    bool cont = true;
    while (cont) {
	int ch = next_char();
	if (ch == '\"') {
	    if (peek_char() == '\"') {
		consume_next_char();
	    } else {
		cont = false;
	    }
	} else if (ch == '\\') {
	    next_escape_sequence();
	} else {
	    if (is_eof()) {
		cont = false;
	    } else {
		add_to_lexeme(ch);
	    }
	}
    }
}

const term_tokenizer::token & term_tokenizer::next_token()
{
    current_.reset();

    if (is_comment_begin()) {
	next_layout_text();
	return current_;
    }

    int ch = peek_char();

    if (is_digit(ch)) {
	set_token_type(TOKEN_NATURAL_NUMBER);
	// Token type can change if we find a '.'
	next_number();
    } else if (ch == '\'') {
	set_token_type(TOKEN_NAME);
	next_quoted_name();
    } else if (is_small_letter(ch)) {
	set_token_type(TOKEN_NAME);
	next_word();
    } else if (is_capital_letter(ch) || is_underline_char(ch)) {
	set_token_type(TOKEN_VARIABLE);
	next_variable();
    } else if (is_layout_char(ch)) {
	set_token_type(TOKEN_LAYOUT_TEXT);
	next_layout_text();
    } else if (is_symbol_char(ch) && !is_full_stop(ch)) { // comment checked
	set_token_type(TOKEN_NAME);
	next_symbol();
    } else if (is_solo_char(ch)) {
	set_token_type(TOKEN_NAME);
	next_solo_char();
    } else if (is_punctuation_char(ch)) {
	set_token_type(TOKEN_PUNCTUATION_CHAR);
	next_punctuation_char();
    } else if (ch == '\"') {
	set_token_type(TOKEN_STRING);
	next_string();
    } else if (is_full_stop(ch)) {
	set_token_type(TOKEN_FULL_STOP);
	next_full_stop();
    }

    return current_;
}

}}


