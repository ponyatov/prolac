#ifndef FEATURE_HH
#define FEATURE_HH
#include "idcapsule.hh"
#include "landmark.hh"
class Namespace;
class Exception;
class Prototype;
class Rule;
class RuleRef;
class Field;
class NodeRef;
class Node;

class Feature {
  
  PermString _basename;
  ModuleID _origin;
  
  Landmark _landmark;
  
 public:
  
  Feature(PermString, ModuleID, const Landmark &);
  Feature(const Feature &);
  virtual ~Feature()				{ }
  
  PermString basename() const			{ return _basename; }
  operator const Landmark &() const		{ return _landmark; }
  const Landmark &landmark() const		{ return _landmark; }
  
  void change_basename(PermString n)		{ _basename = n; }
  
  ModuleID origin() const			{ return _origin; }
  void set_origin(ModuleID o)			{ _origin = o; }
  
  virtual RuleRef *find_ruleref() const		{ return 0; }
  
  virtual void write(Writer &) const;
  virtual void write_full(Writer &) const;
  virtual PermString description() const	{ return "generic"; }
  
  virtual RuleRef *cast_ruleref()		{ return 0; }
  virtual Exception *exception_value() const	{ return 0; }
  virtual Field *cast_field()			{ return 0; }
  virtual NodeRef *cast_noderef()		{ return 0; }
  virtual Namespace *cast_namespace()		{ return 0; }
  virtual Prototype *cast_prototype()		{ return 0; }
  
};


class RuleRef: public Feature {
  
  int _ruleindex;
  Namespace *_contextsp;
  
 public:
  
  RuleRef(PermString n, ModuleID origin, int ri, const Landmark &);
  
  int ruleindex() const			{ return _ruleindex; }
  Namespace *contextsp() const		{ return _contextsp; }
  
  Rule *rule_from(Module *m) const;
  
  void set_ruleindex(int ri)		{ _ruleindex = ri; }
  void set_contextsp(Namespace *ns)	{ _contextsp = ns; }
  
  RuleRef *find_ruleref() const		{ return (RuleRef *)this; }
  
  void write(Writer &) const;
  PermString description() const	{ return "rule"; }
  
  RuleRef *cast_ruleref()		{ return this; }
  Exception *exception_value() const;
  
};


class NodeRef: public Feature {
  
  Node *_node;
  
 public:
  
  NodeRef(PermString, Node *);
  
  Node *node() const			{ return _node; }
  
  void write(Writer &) const;
  PermString description() const	{ return "node"; }
  
  NodeRef *cast_noderef()		{ return this; }
  
};


inline
Feature::Feature(PermString n, ModuleID origin, const Landmark &l)
  : _basename(n), _origin(origin), _landmark(l)
{
}

inline
Feature::Feature(const Feature &f)
  : _basename(f._basename), _origin(f._origin), _landmark(f._landmark)
{
}

inline Writer &
operator<<(Writer &w, Feature *f)
{
  f->write(w);
  return w;
}

#endif
