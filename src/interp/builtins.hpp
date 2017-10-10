#pragma once

#ifndef _interp_builtins_hpp
#define _interp_builtins_hpp

#include "../common/term.hpp"

namespace prologcoin { namespace interp {
	
    class interpreter;
    struct meta_context;

    // We avoid std::function, because it is not as efficient as a
    // "raw" C function pointer.
    typedef bool (*builtin)(interpreter &interp, size_t arity, common::term args[]);

    class builtins {
    public:
	//
	// Profiling
	//

        static bool profile_0(interpreter &interp, size_t arity, common::term args []);

	//
	// Simple
	//

	static bool true_0(interpreter &interp, size_t arity, common::term args[]);

        //
        // Control flow
        //

        static bool operator_comma(interpreter &interp, size_t arity, common::term args[]);
        static bool operator_cut(interpreter &interp, size_t arity, common::term args[]);
        static bool operator_cut_if(interpreter &interp, size_t arity, common::term args[]);
        static bool operator_disjunction(interpreter &interp, size_t arity, common::term args[]);
	static bool operator_arrow(interpreter &interp, size_t arity, common::term args[]);
        static bool operator_if_then(interpreter &interp, size_t arity, common::term args[]);
        static bool operator_if_then_else(interpreter &interp, size_t arity, common::term args[]);

	//
	// Standard order, equality and unification
	//

        // operator @<
        static bool operator_at_less_than(interpreter &interp, size_t arity, common::term args[]);
        // operator @=<
        static bool operator_at_equals_less_than(interpreter &interp, size_t arity, common::term args[]);
        // operator @>
        static bool operator_at_greater_than(interpreter &interp, size_t arity,common::term args[]);
        // operator @>=
        static bool operator_at_greater_than_equals(interpreter &interp, size_t arity, common::term args[]);
        // operator ==
        static bool operator_equals(interpreter &interp, size_t arity, common::term args[]);
        // operator \==
        static bool operator_not_equals(interpreter &interp, size_t arity, common::term args[]);

	// compare/3
	static bool compare_3(interpreter &interp, size_t arity, common::term args[]);

	// operator =
	static bool operator_unification(interpreter &interp, size_t arity, common::term args[]);
	// operator \=
	static bool operator_cannot_unify(interpreter &interp, size_t arity, common::term args[]);

	//
	// Type checks
	//

	// var/1
	static bool var_1(interpreter &interp, size_t arity, common::term args[]);

	// nonvar/1
	static bool nonvar_1(interpreter &interp, size_t arity, common::term args[]);

	// integer/1
	static bool integer_1(interpreter &interp, size_t arity, common::term args[]);

	// number/1
	static bool number_1(interpreter &interp, size_t arity, common::term args[]);

	// atom/1
	static bool atom_1(interpreter &interp, size_t arity, common::term args[]);
	
	// compound/1
	static bool compound_1(interpreter &interp, size_t arity, common::term args[]);
	
	// atomic/1
	static bool atomic_1(interpreter &interp, size_t arity, common::term args[]);

	// callable/1
	static bool callable_1(interpreter &interp, size_t arity, common::term args[]);

	// ground/1
	static bool ground_1(interpreter &interp, size_t arity, common::term args[]);

	// cyclic_term/1
	static bool cyclic_term(interpreter &interp, size_t arity, common::term args[]);

	// acyclic_term/1
	static bool acyclic_term(interpreter &interp, size_t arity, common::term args[]);
	

	//
	// Arithmetics
	//

	static bool is_2(interpreter &interp, size_t arity, common::term args[]);

	//
	// Analyzing & constructing terms
	//

	static bool copy_term_2(interpreter &interp, size_t arity, common::term args[]);
	static bool functor_3(interpreter &interp, size_t arity, common::term args[]);
	static bool operator_deconstruct(interpreter &interp, size_t arity, common::term args[]);
    private:
	static common::term deconstruct_write_list(interpreter &interp,
						   common::term &t,
						   size_t index);
        static bool deconstruct_read_list(interpreter &interp,
					  common::term lst,
					  common::term &t, size_t index);

    public:

	//
	// Meta
	//
	
	static bool operator_disprove(interpreter &interp, size_t arity, common::term args[]);
	static void operator_disprove_post(interpreter &interp, meta_context *context);
    };

}}

#endif


