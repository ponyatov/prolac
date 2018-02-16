#ifndef RESOLVARG_HH
#define RESOLVARG_HH

class ResolveArgs {
  
  bool _is_static: 1;
  bool _is_call: 1;
  bool _is_member: 1;
  bool _allow_type: 1;
  
  ResolveArgs(bool, bool, bool, bool);
  
 public:
  
  ResolveArgs(bool is_static);
  static ResolveArgs type_args();
  
  bool is_static() const		{ return _is_static; }
  bool is_call() const			{ return _is_call; }
  bool is_member() const		{ return _is_member; }
  bool allow_type() const		{ return _allow_type; }
  
  ResolveArgs clear() const;
  ResolveArgs make_static(bool) const;
  ResolveArgs new_call() const;
  ResolveArgs new_member() const;
  ResolveArgs add_allow_type(bool allow_type) const;
  
};


inline
ResolveArgs::ResolveArgs(bool s, bool c, bool m, bool et)
  : _is_static(s), _is_call(c), _is_member(m), _allow_type(et)
{
}

inline
ResolveArgs::ResolveArgs(bool s)
  : _is_static(s), _is_call(0), _is_member(0), _allow_type(0)
{
}

inline ResolveArgs
ResolveArgs::type_args()
{
  return ResolveArgs(true, 0, 0, true);
}


inline ResolveArgs
ResolveArgs::clear() const
{
  return ResolveArgs(_is_static, 0, 0, 0);
}

inline ResolveArgs
ResolveArgs::make_static(bool s) const
{
  return ResolveArgs(s, _is_call, _is_member, _allow_type);
}

inline ResolveArgs
ResolveArgs::new_call() const
{
  return ResolveArgs(_is_static, true, 0, true);
}

inline ResolveArgs
ResolveArgs::new_member() const
{
  return ResolveArgs(_is_static, 0, true, true);
}

inline ResolveArgs
ResolveArgs::add_allow_type(bool allow_type) const
{
  return ResolveArgs(_is_static, 0, 0, allow_type);
}

#endif
