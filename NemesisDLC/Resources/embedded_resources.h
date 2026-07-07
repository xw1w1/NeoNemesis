#ifndef NEMESIS_EMBEDDED_RESOURCES_H
#define NEMESIS_EMBEDDED_RESOURCES_H

namespace Nemesis { namespace Res {

struct Blob { const unsigned char* data; unsigned int size; };

Blob Get(const char* name);
unsigned int Count();
const char* NameAt(unsigned int i);

}}

#endif
