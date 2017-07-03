start :- subterm_1200, full_stop.

subterm_1200 :- term_1200.
subterm_1200 :- term_1000.
subterm_1000 :- term_1000.
subterm_1000 :- term_999.
subterm_999 :- term_999.
subterm_999 :- term_0.
subterm_0 :- term_0.

term_1200 :- unary_op_1200, subterm_1200.
term_1200 :- subterm_1200, bin_op_1200, subterm_1200.

term_1000 :- unary_op_1000, subterm_1000.
term_1000 :- subterm_1000, bin_op_1000, subterm_1000.

term_1000 :- subterm_999, comma, subterm_1000.

term_999 :- unary_op_999, subterm_999.
term_999 :- subterm_999, bin_op_999, subterm_999.

term_0 :- functor_lparen, arguments, rparen.
term_0 :- lparen, subterm_1200, rparen.
term_0 :- lbrace, subterm_1200, rbrace.
term_0 :- list.
term_0 :- string.
term_0 :- constant.
term_0 :- variable.

arguments :- subterm_999.
arguments :- subterm_999, comma, arguments.

list :- lbracket, rbracket.
list :- lbracket, listexpr, rbracket.

listexpr :- subterm_999.
listexpr :- subterm_999, comma, listexpr.
listexpr :- subterm_999, vbar, subterm_999.

constant :- atom.
constant :- number.

number :- unsigned_number.
number :- sign, unsigned_number.
number :- sign, inf.
number :- sign, nan.

unsigned_number :- natural_number.
unsigned_number :- unsigned_float.

atom :- name.

functor_lparen :- name_lparen.

sign :- plus.
sign :- minus.

