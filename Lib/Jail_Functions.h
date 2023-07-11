#ifndef JAIL_FUNCTIONS_H
#define JAIL_FUNCTIONS_H

#include "Jail.h"

namespace JAIL {

/// Register useful functions with the JAIL interpreter
extern void registerFunctions(JAIL::JInterpreter *interpreter);

};

#endif
