#include "term_env.hpp"
#include "term.hpp"
#include "term_ops.hpp"
#include "term_tokenizer.hpp"
#include "term_parser.hpp"
#include "term_emitter.hpp"

namespace prologcoin { namespace common {

class term_env_impl {
public:

  term_env_impl()
  {
    heap_ = new heap();
    heap_owner_ = true;
    ops_ = new term_ops();
    ops_owner_ = true;
    register_hb_ = 0;
    register_h_ = 0;
  }

  ~term_env_impl()
  {
    var_naming_.clear();
    if (heap_owner_) {
      delete heap_;
    }
    if (ops_owner_) {
      delete ops_;
    }
  }

  ext<cell> parse(const std::string &term_expr);
  std::string to_string(const cell &c) const;
  std::string status() const;

  term empty_list() const { return term(*heap_, heap_->empty_list()); }

  inline size_t heap_size() const { return register_h_; }
  inline cell deref(cell c) const { return heap_->deref(c); }

  bool is_list(cell t) const;
  bool is_dotted_pair(cell t) const;
  bool is_empty_list(cell t) const;
  bool equal(cell a, cell b) const;
  bool unify(cell a, cell b);
  bool unify_helper(cell a, cell b);
  void bind(cell a, cell b);
  bool direction();

  inline con_cell functor(cell c) const { return heap_->functor(c); }
  inline bool is_functor(cell c) const { return deref(c).tag() == tag_t::STR; }
  inline cell arg(cell c, size_t index) const { return heap_->arg0(c, index); }
  inline term arg(term t, size_t index) const { return heap_->arg(t, index); }

  inline void push(cell c) const { stack_.push_back(c); }
  inline cell pop() const { cell c = stack_.back(); stack_.pop_back(); return c; }
  inline size_t stack_depth() const { return stack_.size(); }
  inline void trim_stack(size_t depth) const { stack_.resize(depth); }

  inline void trail(size_t index) {
    if (index < register_hb_) {
      // Only record variable bindings that happen before
      // the latest choice point. Otherwise we just trim the
      // heap and wr're done
      push_trail(index);
    }
  }

  inline void push_trail(size_t i) { trail_.push_back(i); }
  inline size_t pop_trail() {auto p=trail_.back(); trail_.pop_back(); return p;}
  inline size_t trail_depth() const { return trail_.size(); }
  void unwind_trail(size_t from, size_t to);
  void trim_trail(size_t to) { trail_.resize(to); }
  
private:
  heap *heap_;
  bool heap_owner_;
  term_ops *ops_;
  bool ops_owner_;

  size_t register_hb_; // Heap size at last choice point
  size_t register_h_;  // Current heap size

  mutable std::vector<cell> stack_;
  std::vector<size_t> trail_;
  std::vector<cell> registers_;
  std::unordered_map<ext<cell>, std::string> var_naming_;
};

ext<cell> term_env_impl::parse(const std::string &term_expr)
{
    std::stringstream ss(term_expr);
    term_tokenizer tokenizer(ss);
    term_parser parser(tokenizer, *heap_, *ops_);
    ext<cell> r = parser.parse();
    // Once parsing is done we'll copy over the var-name bindings
    // so we can pretty print the variable names.
    parser.for_each_var_name( [&](const ext<cell> &ref,
				  const std::string &name)
			      { var_naming_[ref] = name; } );
    register_h_ = heap_->size();
    return r;
}

std::string term_env_impl::to_string(const cell &c) const
{
    cell dc = deref(c);
    std::stringstream ss;
    term_emitter emitter(ss, *heap_, *ops_);
    emitter.set_var_naming(var_naming_);
    emitter.print(dc);
    return ss.str();
}


std::string term_env_impl::status() const
{
    std::stringstream ss;
    ss << "term_env::status() { heap_size=" << register_h_ 
       << ",stack_size=" << stack_depth() << ",trail_size=" << trail_depth() <<"}";
    return ss.str();
}


//
// This is the basic unification algorithm for two terms.
//

bool term_env_impl::is_list(cell t) const
{
    return heap_->is_list(t);
}

bool term_env_impl::is_dotted_pair(cell t) const
{
    return heap_->is_dotted_pair(t);
}

bool term_env_impl::is_empty_list(cell t) const
{
    return heap_->is_empty_list(t);
}

bool term_env_impl::equal(cell a, cell b) const
{
    size_t d = stack_depth();

    push(b);
    push(a);

    while (stack_depth() > d) {
	a = deref(pop());
	b = deref(pop());

	if (a == b) {
	    continue;
	}
	
	if (a.tag() != b.tag()) {
	    trim_stack(d);
	    return false;
	}

	if (a.tag() != tag_t::STR) {
	    trim_stack(d);
	    return false;
	}

	con_cell fa = functor(a);
	con_cell fb = functor(b);

	if (fa != fb) {
	    trim_stack(d);
	    return false;
	}

	str_cell &astr = static_cast<str_cell &>(a);
	str_cell &bstr = static_cast<str_cell &>(b);

	size_t num_args = fa.arity();
	for (size_t i = 0; i < num_args; i++) {
	    auto ai = arg(astr, num_args-i-1);
	    auto bi = arg(bstr, num_args-i-1);
	    push(bi);
	    push(ai);
	}
    }

    return true;
}

bool term_env_impl::unify(cell a, cell b)
{
    size_t start_trail = trail_depth();
    size_t start_stack = stack_depth();

    size_t old_register_hb = register_hb_;

    register_hb_ = register_h_;

    bool r = unify_helper(a, b);

    if (!r) {
      unwind_trail(start_trail, trail_depth());
      trim_trail(start_trail);
      trim_stack(start_stack);

      register_hb_ = old_register_hb;
      return false;
    }

    register_hb_ = old_register_hb;
    return true;
}

// Bind 'a' to 'b'.
void term_env_impl::bind(cell a, cell b)
{
    // We know 'a' is a REF cell. REF cell are always on
    // heap in our version.
    ref_cell &rc = static_cast<ref_cell &>(a);
    size_t index = rc.index();
    cell &p = (*heap_)[index];
    p = b;
    trail(index);
}

void term_env_impl::unwind_trail(size_t from, size_t to)
{
    for (size_t i = from; i < to; i++) {
      size_t index = trail_[i];
      (*heap_)[index] = ref_cell(index);
    }
}

bool term_env_impl::unify_helper(cell a, cell b)
{
    size_t d = stack_depth();

    push(b);
    push(a);

    while (stack_depth() > d) {
        a = deref(pop());
	b = deref(pop());

	if (a == b) {
	    continue;
	}

	// If at least one of them is a REF, then bind it.
	if (a.tag() == tag_t::REF) {
  	    if (b.tag() == tag_t::REF) {
	      auto ra = static_cast<ref_cell &>(a);
	      auto rb = static_cast<ref_cell &>(b);
	      // It's more efficient to bind higher addresses
	      // to lower if there's a choice. That way we
	      // don't need to trail the bindings.
	      if (ra.index() < rb.index()) {
		bind(b, a);
	      } else {
		bind(a, b);
	      }
	      continue;
	    } else {
	      bind(a, b);
	      continue;
	    }
	} else if (b.tag() == tag_t::REF) {
	    bind(b, a);
	    continue;
	}

	// Check tags
	if (a.tag() != b.tag()) {
	    return false;
	}
	
	switch (a.tag()) {
	case tag_t::CON:
	case tag_t::INT:
	  if (a != b) {
	    return false;
	  }
	case tag_t::STR: {
	  str_cell &astr = static_cast<str_cell &>(a);
	  str_cell &bstr = static_cast<str_cell &>(b);
	  con_cell f = functor(astr);
	  if (f != functor(bstr)) {
	    return false;
	  }
	  // Push pairwise args
	  size_t num_args = f.arity();
	  for (size_t i = 0; i < num_args; i++) {
	    auto ai = arg(astr, num_args-i-1);
	    auto bi = arg(bstr, num_args-i-1);
	    push(bi);
	    push(ai);
	  }
	  break;
	}
        // TODO: Implement these two later...
	case tag_t::BIG:
	case tag_t::GBL: assert(false); break;
	}
    }

    return true;
}

//
// term_env
//

term_env::term_env()
{
    impl_ = new term_env_impl();
}

term_env::~term_env()
{
    delete impl_;
}

ext<cell> term_env::parse(const std::string &term_expr)
{
    return impl_->parse(term_expr);
}

std::string term_env::to_string(const term  &term) const
{
    return impl_->to_string(term);
}

std::string term_env::status() const
{
    return impl_->status();
}

size_t term_env::stack_size() const
{
    return impl_->stack_depth();
}

size_t term_env::trail_size() const
{
    return impl_->trail_depth();
}

size_t term_env::heap_size() const
{
    return impl_->heap_size();
}

bool term_env::is_list(const term &t) const
{
    return impl_->is_list(*t);
}

bool term_env::is_dotted_pair(const term &t) const
{
    return impl_->is_dotted_pair(*t);
}

bool term_env::is_empty_list(const term &t) const
{
    return impl_->is_empty_list(t);
}

term term_env::empty_list() const
{
    return impl_->empty_list();
}

con_cell term_env::functor(const term &t)
{
    return impl_->functor(*t);
}

bool term_env::is_functor(const term &t, con_cell f)
{
    return functor(t) == f;
}

bool term_env::is_functor(const term &t)
{
    return impl_->is_functor(t);
}

term term_env::arg(const term &t, size_t index)
{
    return impl_->arg(t, index);
}

bool term_env::unify(term &a, term &b)
{
    return impl_->unify(a,b);
}

bool term_env::equal(const term &a, const term &b)
{
    return impl_->equal(a,b);
}


}}

