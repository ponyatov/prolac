#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "landmark.hh"
#include "writer.hh"

Writer &
operator<<(Writer &w, const Landmark &l)
{
  w << l.file() << ":" << l.line() << ": ";
  return w;
}
