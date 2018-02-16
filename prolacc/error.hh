#ifndef ERROR_HH
#define ERROR_HH
class Landmark;
class Namespace;

bool error(const Landmark &, const char *, ...);
bool warning(const Landmark &, const char *, ...);
void set_error_context(const char *, ...);
void reset_error_context();

extern int num_errors;
extern int num_warnings;

#endif
