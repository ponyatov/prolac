#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "module.hh"
#include "error.hh"
#include "ruleset.hh"
#include "node.hh"
#include "field.hh"
#include "node.hh"
#include "compiler.hh"
#include "prototype.hh"
#include <cassert>

/*****
 * ModuleNames
 **/

ModuleNames::ModuleNames(PermString n, Namespace *parent_space,
			 Protomodule *protomodule, const Landmark &lm)
  : Namespace(n, lm),
    _prototype(protomodule), _parent_space(parent_space),
    _module(new Module(protomodule, this)), _own_module(true),
    _using(0), _explicit_using(false),
    _inliners(-1), _module_inliners(inlineDefault),
    _from_equation(false), _trivial_equation(0),
    _pointer_type(new PointerType(this))
{
  Namespace::set_origin(_module);
  _module->make_self();
}

ModuleNames::ModuleNames(const ModuleNames &mn, Prototype *prototype)
  : Namespace(mn, mn._parent_space),
    _prototype(prototype), _parent_space(mn._parent_space),
    _module(mn._module), _own_module(false),
    _using(mn._using), _explicit_using(mn._explicit_using),
    _inliners(-1), _module_inliners(inlineDefault),
    _from_equation(false), _trivial_equation(0),
    _pointer_type(new PointerType(this))
{
  // Don't copy inliners yet. Do that below in copy_inliners.
  // Why? ModuleNames are copied after resolve1(); inliners aren't ready
  // until after resolve2(). Need to double-check on this.
}


void
ModuleNames::force_equation(PermString n, Namespace *new_parent)
{
  force_trivial_equation(n, new_parent, 0);
}

void
ModuleNames::force_trivial_equation(PermString n, Namespace *new_parent,
				    ModuleNames *trivial)
{
  assert(trivial != this);
  change_basename(n);
  _parent_space = new_parent;
  _from_equation = true;
  _trivial_equation = trivial;
}

void
ModuleNames::copy_inliners(const ModuleNames &mn)
{
  _inliners = mn._inliners;
  _module_inliners = mn._module_inliners;
}

bool
ModuleNames::allow_import() const
{
  return true;
}

bool
ModuleNames::allow_ancestor() const
{
  return true;
}


void
ModuleNames::resolve1_parent(ModuleNames *mn)
{
    for (HashMap<PermString, int>::iterator i = mn->_using.begin(); i != mn->_using.end(); i++)
	// error on conflict?
	_using.insert(i.key(), i.value());
}


/*****
 * using
 **/

bool
ModuleNames::is_using_empty(bool explicit_is_full) const
{
    if (!_explicit_using)
	return !explicit_is_full;
    else
	return _using.empty() && _using.default_value() == 0;
}

void
ModuleNames::set_using(PermString n, bool isu)
{
    _using.insert(n, isu ? 2 : 0);
    _explicit_using = true;
}

void
ModuleNames::clear_using(bool isu)
{
    HashMap<PermString, int> new_using(isu ? 2 : 0);
    _using = new_using;
    _explicit_using = true;
}

void
ModuleNames::set_using_all_static()
{
    HashMap<PermString, int> new_using(1);
  
    for (HashMap<PermString, int>::iterator i = _using.begin(); i != _using.end(); i++)
	if (i.value() == 2)
	    new_using.insert(i.key(), i.value());
  
    _using = new_using;
    _explicit_using = true;
}


bool
ModuleNames::has_implicit_feature(PermString n, bool want_static) const
{
  int using_value = _using[n];
  if (using_value == 0)
    return false;
  
  bool found = false;
  bool found_static = false;
  if (Feature *feat = find(n)) {
    if (RuleRef *rref = feat->find_ruleref()) {
      Rule *rule = _module->find_rule(rref);
      found = true;
      found_static = rule->is_static();
    } else
      warning(*feat, "found `%F' in implicit method search", feat);
  }
  
  // Only return `true' if it's the right kind of rule (static or dynamic).
  if (!found)
    return false;
  else if (want_static)
    return found_static;
  else
    return using_value == 2 && !found_static;
}


/*****
 * inlining
 **/

int
ModuleNames::inline_level(Rule *rule) const
{
  int level = _inliners[rule];
  if (level < 0)
    level = _module_inliners[rule->actual()];
  return level;
}

void
ModuleNames::set_inline_level(Rule *rule, int level)
{
  _inliners.insert(rule, level);
}

void
ModuleNames::clear_inline(ModuleID module, int level)
{
  for (HashMap<RuleID, int>::iterator i = _inliners.begin(); i != _inliners.end(); i++)
      if (i.key()->actual() == module)
	  _inliners.insert(i.key(), -1);
  _module_inliners.insert(module, level);
}


/*****
 * type
 **/

int
ModuleNames::size() const
{
  return _module->size();
}

int
ModuleNames::align() const
{
  return _module->align();
}


/*****
 * gen_name
 **/

PermString
ModuleNames::gen_name() const
{
  PermString parent = _parent_space->gen_name();
  if (parent)
    return permprintf("%p__%p", parent.capsule(), basename().capsule());
  else
    return basename();
}


/*****
 * Module
 **/

Module::Module(Protomodule *p, ModuleNames *mn)
  : _protomodule(p), _base_modnames(mn), _default_modnames(mn),
    _receiver_class(0), _parent_class(0),
    _gen_name(mn->gen_name()),
    _ancestor_map(-1), _field_map(-1),
    _anc_field_count(0), _self_field(0),
    _self_vtbl_field(0), _size(-1), _align(-1),
    _module_map(-1)
{
}

void
Module::merge_receiver_class(Module *m)
{
  if (_receiver_class->has_ancestor(m))
    _receiver_class = m;
  else if (m->has_ancestor(_receiver_class))
    /* nada */;
  else {
    /* closest common ancestor */
    Module *new_m = 0;
    int i = 0;
    ModuleID test_m;
    while (m->each_ancestor(i, test_m))
      if (_receiver_class->has_ancestor(test_m)
	  && (!new_m || test_m->has_ancestor(new_m)))
	new_m = test_m;
    _receiver_class = new_m;
  }
}


/*****
 * finding
 **/

Ruleset *
Module::find_ruleset(ModuleID mid) const
{
  int mi = _ancestor_map[mid];
  if (mi >= 0) return _rsets[mi];
  else return 0;
}

Rule *
Module::find_rule(ModuleID mid, int ri) const
{
  int mi = _ancestor_map[mid];
  if (mi >= 0)
    return _rsets[mi]->rule(ri);
  else
    return 0;
}

Rule *
Module::find_rule(const RuleRef *rref) const
{
  return find_rule(rref->origin(), rref->ruleindex());
}

Rule *
Module::find_rule(const Rule *rule) const
{
  return find_rule(rule->origin(), rule->ruleindex());
}

bool
Module::each_ancestor(int &eacher, ModuleID &result) const
{
  if (eacher < 0 || eacher >= _rsets.size())
    return false;
  result = _rsets[eacher]->origin();
  eacher++;
  return true;
}


/*****
 * make self-related fields
 **/

Field *
Module::make_self()
{
  assert(!_self_field);
  _self_field =
    Field::new_ancestor(this, basename(), _base_modnames, landmark());
  return _self_field;
}

Field *
Module::self_vtbl_field()
{
  if (!_self_vtbl_field) {
    PermString name = permprintf("__vtbl_%p", gen_module_name().capsule());
    _self_vtbl_field = Field::new_slot(this, 0, name, false, *this);
    _self_vtbl_field->set_type
      (self_ruleset()->gen_type()->make_pointer());
    _fields.push_back(_self_vtbl_field);
  }
  return _self_vtbl_field;
}


/*****
 * Type
 **/

PointerType *
ModuleNames::make_pointer(const Landmark &) const
{
  return _pointer_type;
}

bool
ModuleNames::conforms_to(const Type *t) const
{
  return _module == t->cast_module();
}

bool
ModuleNames::same_type(const Type *t) const
{
  if (Module *m = t->cast_module())
    return m == _module;
  else
    return false;
}

Node *
ModuleNames::convert_to(Node *n, const Landmark &lm, bool cast) const
{
  const Type *from = n->type();
  if (from == this)
    return n;
  
  // Allow conversions to ancestors.
#if 0
  if (Module *mod_from = from->cast_module())
    if (mod_from->has_ancestor(_module))
      return n->make_cast(this);
    else if (!cast) {
      // Special error message only if we're not casting.
      error(lm, "can't convert: `%t' not an ancestor of `%t'", this, from);
      return n;
    }
#endif
  
  return convert_error(n, lm, cast);
}

Node *
ModuleNames::convert_pointer_to(Node *n, ModuleNames *from,
				const Landmark &lm, bool cast) const
{
  if (from->_module == _module)
    // XXX: same module, different module operators?
    return n;
  else if (from->_module->has_ancestor(_module))
    // Allow conversions to ancestors.
    return n->make_cast(_pointer_type, lm);
  else if (!cast) {
    // Special error message only if we're not casting.
    error(lm, "can't convert pointer: `%t' not an ancestor of `%t'",
	  (Type *)this, (Type *)from);
    return n;
  }
  
  return _pointer_type->convert_error(n, lm, cast);
}

Node *
ModuleNames::cast_pointer_to(Node *n, ModuleNames *from, const Landmark &lm) const
{
  if (Node *result = convert_pointer_to(n, from, lm, true))
    return result;
  
  // Allow casts to descendents.
  if (_module->has_ancestor(from->_module))
    return n->make_cast(_pointer_type, lm);

  // Allow other casts too!
  return n->make_cast(_pointer_type, lm);
  
  /* return _pointer_type->cast_error(n); */
}

void
Module::check_override_types()
{
  for (int ri = 0; ri < _rsets.size(); ri++) {
    Ruleset *ruleset = _rsets[ri];
    if (ruleset->actual() == this)
      ruleset->check_types();
  }
}
    
Type *
ModuleNames::header_type(Type *t) const
{
  if (ModuleNames *mn = t->cast_modnames())
    return header_module(mn);
  // also check for compound types
  if (ArrayType *at = t->cast_array()) {
    Type *base_header_type = header_type(at->deref());
    if (base_header_type != at->deref())
      return ArrayType::make(base_header_type, at->nelem(), Landmark());
  } else if (PointerType *pt = t->cast_pointer()) {
    Type *base_header_type = header_type(pt->deref());
    if (base_header_type != pt->deref())
      return base_header_type->make_pointer();
  }
  // otherwise, header type is no different from our type
  return t;
}

ModuleNames *
Module::header_module(ModuleNames *mn) const
{
  int i = _module_map[mn->module()];
  if (i >= 0)
    return _filtered_modules[i];
  else
    return mn;
}

void
Module::grep_my_fields(Vector<Field *> &vec) const
{
  for (int i = 0; i < _fields.size(); i++)
    if (_fields[i]->origin() == actual()
	&& _fields[i] != _self_field
	&& _fields[i] != _self_vtbl_field)
      vec.push_back(_fields[i]);
}

void
Module::grep_my_rules(Vector<Rule *> &vec) const
{
  for (int i = 0; i < _rsets.size(); i++)
    for (int r = 0; r < _rsets[i]->count(); r++) {
      Rule *rule = _rsets[i]->rule(r);
      if (rule->actual() == this)
	vec.push_back(rule);
    }
}


/*****
 * resolve etc.
 **/

bool
Module::merge_rulesets(int rsi1, Ruleset *r2)
{
  assert(r2->actual() != this);
  
  Ruleset *r1 = _rsets[rsi1];
  Module *m1 = r1->actual();
  Module *m2 = r2->actual();
  assert(m1 != this && r1->origin() == r2->origin());
  
  // This is all bullshit because of redefinitions!
  
  if (m1 == m2)
    // Identical Modules; this is OK.
    return true;
  
  else if (m1->has_ancestor(m2))
    // The old version is a subtype of the new; leave as is.
    return true;
  
  else if (m2->has_ancestor(m1)) {
    // The new version is a subtype of the old; install the new.
    _rsets[rsi1] = r2;
    return true;
    
  } else {
    error(*this, "bad multiple inheritance (base conflict):");
    error(*this, "  `%m' and `%m' both redefine base `%m'",
	  m1, m2, r1->origin().obj());
    return false;
  }
}

bool
Module::merge_fields(int fi1, Field *f2)
{
  assert(f2->origin() != this);
  
  Field *f1 = _fields[fi1];
  Module *m1 = f1->origin();
  Module *m2 = f2->origin();
  assert(m1 != this && m1 == m2);
  
  if (m1 == m2)
    // Identical Modules; this is OK.
    return true;
  
  else if (m1->has_ancestor(m2))
    // The old version is a subtype of the new; leave as is.
    return true;
  
  else if (m2->has_ancestor(m1)) {
    // The new version is a subtype of the old; install the new.
    _fields[fi1] = f2;
    return true;
    
  } else {
    if (!f1->is_ancestor()) {
      // Don't need to report ancestor errors; merge_rulesets did that.
      error(*this, "bad multiple inheritance (%-f conflict):", f1);
      error(*this, "  `%m' and `%m' both redefine `%f' from base `%m'",
	    m1, m2, f1, f1->origin().obj());
    }
    return false;
  }
}

bool
Module::resolve1_parent(Module *m)
{
  bool ok = true;
  
  for (int ri = 0; ri < m->_rsets.size(); ri++) {
    Ruleset *rs = m->_rsets[ri];
    int anc = _ancestor_map[ rs->origin() ];
    
    if (anc != -1) {
      if (!merge_rulesets(anc, rs))
	ok = false;
    } else {
      anc = _rsets.size();
      _rsets.push_back(rs);
      _ancestor_map.insert( rs->origin(), anc );
    }
  }
  
  assert(_anc_field_count == _fields.size());
  for (int fi = 0; fi < m->_anc_field_count; fi++) {
    Field *field = m->_fields[fi];
    assert(field->is_ancestor());
    
    int fi = _field_map[field];
    
    if (fi != -1) {
      if (!merge_fields(fi, field))
	ok = false;
    } else {
      fi = _fields.size();
      _fields.push_back(field);
      _field_map.insert(field, fi);
      _anc_field_count++;
    }
  }
  
  return ok;
}

bool
Module::resolve1_self()
{
  // Add on our ruleset.
  Ruleset *selfrs = new Ruleset(this);
  int selfanc = _rsets.size();
  _rsets.push_back(selfrs);
  _ancestor_map.insert(this, selfanc);
  
  // Add space for the constructor.
  selfrs->append(0);
  
  // Self-reference
  Field *self = _self_field;
  assert(_anc_field_count == _fields.size());
  _field_map.insert(self, _anc_field_count);
  _fields.push_back(self);
  _anc_field_count++;
  
  return true;
}

bool
Module::resolve1_rule(Rule *rule, RuleRef *rref)
{
  int anc = _ancestor_map[rule->origin()];
  assert(anc != -1);
  Ruleset *rs = _rsets[anc];
  
  if (rule->origin() == this) {
    // A new rule; append it to ourself.
    int newri = rs->append(rule);
    rref->set_ruleindex(newri);
    rule->set_origin(this, newri);
    
  } else {
    // An old rule; override it.
    
    // Ensure the Ruleset really belongs to us.
    if (rs->actual() != this)
      rs = _rsets[anc] = new Ruleset(rs, this);
    
    // Check the current Rule to make sure we're not overriding something more
    // than once (probably you would do this through accidentally through
    // different names).
    Rule *old_rule = rs->rule(rule->ruleindex());
    if (old_rule->actual() == actual()) {
      Rule *orig_rule = old_rule->origin()->find_rule(old_rule);
      error(*rule, "overriding `%r' method more than once", orig_rule);
    }
    
    rs->override(rule->ruleindex(), rule);
  }
  
  return true;
}

void
Module::resolve1_constructor(Rule *rule, RuleRef *rref)
{
  rule->make_constructor();
  rule->set_origin(actual(), 0);
  
  Ruleset *srs = self_ruleset();
  srs->fill_in(0, rule);
  rref->set_ruleindex(0);
}

bool
Module::resolve1_field(Field *field)
{
  assert(field->origin() == this);
  int fi = _fields.size();
  _fields.push_back(field);
  _field_map.insert(field, fi);
  return true;
}


bool
Module::resolve2_parent(Module *m)
{
  bool ok = true;
  // Make sure m's self_vtbl_field exists.
  m->self_vtbl_field();
  assert(!_parent_class);
  _parent_class = m;
  
  // Add all of m's fields to ours.
  for (int fi = 0; fi < m->_fields.size(); fi++) {
    Field *field = m->_fields[fi];
    int fi = _field_map[field];
    
    if (fi != -1) {
      if (!merge_fields(fi, field))
	ok = false;
    } else {
      fi = _fields.size();
      _fields.push_back(field);
      _field_map.insert(field, fi);
    }
  }
  
  for (int i = 0; i < m->_filtered_modules.size(); i++) {
    ModuleNames *mn = m->_filtered_modules[i];
    int mod_i = _module_map[mn->module()];
    if (mod_i < 0) {
      mod_i = _filtered_modules.size();
      _filtered_modules.push_back(mn);
      _module_map.insert(mn->module(), mod_i);
    }
  }
  
  return ok;
}


void
Module::insert_module_map(ModuleNames *mn)
{
  ModuleID m = mn->module();
  int i = _module_map[m];
  if (i >= 0)
    _filtered_modules[i] = mn;
  else {
    i = _filtered_modules.size();
    _filtered_modules.push_back(mn);
    _module_map.insert(m, i);
  }
}


void
Module::resolve2_module_map()
{
  for (int i = 0; i < _fields.size(); i++) {
    Field *f = _fields[i];
    if (f->origin() == this && !f->is_slot())
      if (ModuleNames *mn = f->modnames())
	insert_module_map(mn);
  }
}


void
Module::set_default_modnames(ModuleNames *mn)
{
  insert_module_map(mn);
  _default_modnames = mn;
}


void
Module::add_field(Field *f)
{
  int fi = _field_map[f];
  if (fi >= 0)
    _fields[fi] = f;
  else {
    fi = _fields.size();
    _fields.push_back(f);
    _field_map.insert(f, fi);
  }
}


/*****
 * layout
 **/

static bool
set_if_prefix(Vector<Module *> &out, const Vector<Module *> &in)
{
  for (int i = 0; i < out.size() && i < in.size(); i++)
    if (out[i]->actual() != in[i]->actual())
      return false;
  if (out.size() < in.size())
    out = in;
  return true;
}

void
Module::layout1()
{
  if (_anc_layout.size())
    return;
  
  //errwriter << "laying out " << (void *)this << "\n";
  //if (_actual != _version)
  //_actual->layout1();
  
  /**
   * 1. Calculate ancestors' layout. */
  
  assert(_fields[_anc_field_count - 1] == _self_field);
  Vector< Vector<Module *> > prefixes;
  for (int i = 0; i < _anc_field_count - 1; i++) {
    Module *min = _fields[i]->type()->cast_module();
    min->layout1();
    const Vector<Module *> &in = min->_anc_layout;
    
    for (int j = 0; j < prefixes.size(); j++)
      if (set_if_prefix(prefixes[j], in))
	goto found;
    prefixes.push_back(in);
    
   found: ;
  }

  // Figure out if we need to generate a structure.
  
  // We definitely need to if we have no ancestors -- or multiple disjoint
  // ancestors.
  _need_struct = prefixes.size() != 1;
  
  // ...or we have virtual functions;
  if (self_ruleset()->any_dynamic())
    _need_struct = true;
  
  // ...or slots;
  for (int i = _anc_field_count; i < _fields.size(); i++)
    if (_fields[i]->is_slot() && _fields[i]->origin() == this) {
      _need_struct = true;
      break;
    }
  
  // If we need to generate a structure...
  if (_need_struct) {
    if (prefixes.size() == 0) // No ancestors
      prefixes.push_back( Vector<Module *>() );
    
    // Find longest prefix, and append ourself to that.
    int max = 0;
    for (int j = 1; j < prefixes.size(); j++)
      if (prefixes[j].size() > prefixes[max].size())
	max = j;
    prefixes[max].push_back(this);
  }
  
  // Set our ancestor layout to `prefixes', flattened.
  assert(prefixes.size() == 1 && "multiple inheritance not supported");
  _anc_layout = prefixes[0];
  
  // Set up `_all_slots'. Can't use the convenient `_fields' array
  for (int i = _anc_field_count; i < _fields.size(); i++) {
    Field *f = _fields[i];
    if (f->is_slot() && f->is_in_object()) {
      Module *m = f->type()->vtbl_of();
      if (!m || m->self_ruleset()->any_dynamic())
	_all_slots.push_back(f);
    }
  }
  
  // Make the names.
  if (_need_struct)
    _object_name = gen_module_name();
  else
    _object_name = prefixes[0].back()->gen_object_name();
}

static int
field_offset_sorter(const void *v1, const void *v2)
{
  const Field *f1 = *((const Field **)v1);
  const Field *f2 = *((const Field **)v2);
  if (f1->offset() == f2->offset())
    return 0;
  else if (f1->offset() < 0)
    return 1;
  else if (f2->offset() < 0)
    return -1;
  else
    return f1->offset() - f2->offset();
}

void
Module::layout2()
{
  if (_size >= 0)
    return;
  
  _size = 0;
  _align = 1;
  if (!_all_slots.size())
    return;
  
  // layout2() ALL important modules, to get sizes for slot module types
  for (int i = 0; i < _fields.size(); i++)
    if (_fields[i]->is_in_object()) {
      Module *min = _fields[i]->type()->cast_module();
      if (min) min->layout2();
    }
  
  // arrange slots in offset order, with unknown-offset slots at the end
  qsort(&_all_slots[0], _all_slots.size(), sizeof(Field *),
	field_offset_sorter);
  
  // find the first `floating' slot (a slot with unknown offset)
  int fixed_i = 0;
  int float_i = 0;
  while (float_i < _all_slots.size()
	 && _all_slots[float_i]->offset() >= 0)
    float_i++;
  int first_float = float_i;
  
  // place each floating slot
  int offset = 0;
  while (float_i < _all_slots.size()) {
    int s = _all_slots[float_i]->size();
    int a = _all_slots[float_i]->align();
    assert(s >= 0 && a > 0);
    
    if (s > 0) {
      int align_mask = a - 1;
      if ((offset & align_mask) != 0)
	offset = (offset + a) & ~align_mask;
      if (fixed_i < first_float
	  && offset + s > _all_slots[fixed_i]->offset()) {
	offset = _all_slots[fixed_i]->offset() + _all_slots[fixed_i]->size();
	fixed_i++;
	continue;
      }
    }
    
    _all_slots[float_i]->set_offset(offset);
    offset += _all_slots[float_i]->size();
    float_i++;
  }
  
  // find the structure's alignment and size
  for (int i = 0; i < _all_slots.size(); i++)
    if (_all_slots[i]->align() > _align)
      _align = _all_slots[i]->align();
  
  Field *last_slot = _all_slots[_all_slots.size() - 1];
  _size = last_slot->offset() + last_slot->size();
  int align_mask = _align - 1;
  if ((_size & align_mask) != 0)
    _size = (_size + _align) & ~align_mask;
  
  // reorder slots in offset order
  qsort(&_all_slots[0], _all_slots.size(), sizeof(Field *),
	field_offset_sorter);
  
  // check for misalignment or overlap (only relevant for user-placed slots)
  for (int i = 0; i < _all_slots.size(); i++) {
    Field *slot = _all_slots[i];
    bool mine = slot->origin() == this;
    if (mine && (slot->offset() & (slot->align() - 1)) != 0)
      error(*slot, "placement of slot `%m.%f' is unaligned",
	    slot->origin().obj(), slot);
    
    if (i == _all_slots.size() - 1) break;
    Field *next = _all_slots[i+1];
    bool next_mine = next->origin() == this;
    if (slot->offset() + slot->size() > next->offset())
      if (mine || next_mine) {
	error(*slot, "placement of slot `%m.%f' overlaps slot `%m.%f'",
	      slot->origin().obj(), slot, next->origin().obj(), next);
	error(*next, "(`%m.%f' was defined here)", next->origin().obj(), next);
      }
  }
}


/*****
 * gen
 **/

void
Module::gen(Writer &w) const
{
  assert(_object_name);
  w << gen_object_name();
}

int
ModuleNames::gen_parts(Writer &w, PermString &) const
{
  _module->gen(w);
  return 0;
}

void
ModuleNames::gen_prototype(Writer &w) const
{
  if (_gen_track.gen_if()) {
    _module->mark_gen();
    _module->gen_prototype(w);
    if (_from_equation)
      w << "typedef " << _module->gen_object_name()
	<< " " << gen_name() << ";\n";
  }
}

void
ModuleNames::gen_create(Writer &w) const
{
  gen_prototype(w);
  _module->gen_struct(w);
}


void
Module::gen_slot_types(Writer &w) const
{
  for (int i = _anc_field_count; i < _fields.size(); i++)
    if (_fields[i]->is_slot())
      _fields[i]->type()->gen_create(w);
}


void
Module::gen_slots(Writer &w) const
{
  int offset = 0;
  int npadding = 0;
  for (int i = 0; i < _all_slots.size(); i++) {
    Field *slot = _all_slots[i];
    if (!slot->compiled_name()) continue;
    
    int o = offset;
    int align_mask = slot->align() - 1;
    if ((o & align_mask) != 0)
      o = (o + slot->align()) & ~align_mask;
    if (o != slot->offset()) {
      w << "char __padding_" << npadding << "["
	<< (slot->offset() - offset) << "];\n";
      npadding++;
    }
    
    slot->type()->gen_object(w, slot->compiled_name());
    w << ";" << wmtab(40) << "/* " << slot->origin() << " */\n";
    
    offset = slot->offset() + slot->size();
  }
  
  // Vtbl was taken care of by adding a fake slot to _all_slots
}

void
Module::gen_static_slots(Writer &w) const
{
  for (int i = _anc_field_count; i < _fields.size(); i++)
    if (_fields[i]->is_slot() && _fields[i]->origin() == this
	&& _fields[i]->is_static()) {
      if (PermString name = _fields[i]->compiled_name()) {
	_fields[i]->type()->gen_object(w, name);
	w << ";" << wmtab(40) << "/* " << this
	  << " " << _fields[i]->offset()
	  << " " << _fields[i]->size()
	  << "." << _fields[i]->align()
	  << " */\n";
      }
    }
}

void
Module::gen_prototype(Writer &w) const
{
  if (_gen_track.protogen_if()) {
    for (int i = 0; i < _anc_layout.size(); i++)
      if (_anc_layout[i] != this)
	_anc_layout[i]->gen_prototype(w);
    if (_need_struct)
      w << "typedef struct " << gen_module_name() << " " << gen_module_name() << ";\n";
    else
      w << "typedef " << gen_object_name() << " " << gen_module_name() << ";\n";
  }
}

void
Module::gen_struct(Writer &w) const
{
  gen_prototype(w);
  if (!_gen_track.gen_if())
    return;
  
  //// Generate the structure for our personal vtbl
  Ruleset *self_rs = self_ruleset();
  if (self_rs->any_dynamic())
    self_rs->gen_struct(w);
  
  for (int ai = 0; ai < _anc_layout.size(); ai++)
    if (_anc_layout[ai] != this)
      _anc_layout[ai]->gen_struct(w);
  
  //// Generate our structure
  if (_need_struct) {
    gen_slot_types(w);
    w << "struct " << gen_module_name() << " {\n" << wmindent(2);
    gen_slots(w);
    w << wmindent(-2) << "};\n";
  }
  
  gen_static_slots(w);
}

void
Module::gen_vtbl_proto(Writer &w) const
{
  //// Generate the prototypes for our virtual tables
  for (int i = 0; i < _rsets.size(); i++)
    if (_rsets[i]->any_dynamic())
      _rsets[i]->gen_item_proto(w);
}

void
Module::gen_vtbl(Writer &w, ModuleNames *outer_module) const
{
  //// Generate the data for our virtual tables
  for (int i = 0; i < _rsets.size(); i++)
    if (_rsets[i]->any_dynamic())
      _rsets[i]->gen_item(w, outer_module);
  //c->out << "\n";
}

void
Module::gen_assign_vtbl(Compiler *c, Node *value) const
{
  for (int i = 0; i < _anc_field_count; i++) {
    Ruleset *rs = _rsets[i];
    if (rs->any_dynamic()) {
      c->out << "(";
      value->gen_value_real(c, gctxPointerCast); // don't generate casts
      c->out << ")." << rs->origin()->gen_self_vtbl_name() << " = &";
      rs->gen_item_name(c->out);
      c->out << ";\n";
    }
  }
}

PermString
Module::gen_self_vtbl_name()
{
  return self_vtbl_field()->basename();
}


/*****
 * write
 **/

void
Module::write(Writer &w) const
{
  _base_modnames->write(w);
}

void
ModuleNames::write(Writer &w) const
{
  if (!this)
    errwriter << "[NULL]";
  else if (_trivial_equation)
    _trivial_equation->write(w);
  else {
    write_super(w);
    w << basename();
  }
}

void
ModuleNames::write_super(Writer &w) const
{
  if (!this)
    errwriter << "[NULL]";
  else if (_parent_space->parent())
    w << _parent_space << '.';
}


void
ModuleNames::write_full(Writer &w, bool do_namespace, bool do_rulesets)
{
  write(w);
  w << " ((\n" << wmindent(2);
  w["modnames"] = this;
  
  if (do_namespace) {
    Namespace::write_full(w);
    w << wmendl;
  }
  if (do_rulesets)
    _module->write_rulesets(w);
  
  w["modnames"] = 0;
  w << wmindent(-2) << "))\n\n";
}


void
Module::write_rulesets(Writer &w) const
{
  for (int i = 0; i < _rsets.size(); i++)
    _rsets[i]->write_full(w);
}
