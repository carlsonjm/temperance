// SPDX-License-Identifier: LGPL-2.0-or-later
#pragma once

#include <Plasma/Corona>

// Plasma 6.8 numbers screens with uint where 6.7 used int; a test corona
// overrides screenGeometry with whichever its Plasma declares.
template<typename> struct CoronaScreenArgument;
template<typename Class, typename Result, typename Argument>
struct CoronaScreenArgument<Result (Class::*)(Argument) const> {
    using type = Argument;
};
using CoronaScreenId = CoronaScreenArgument<decltype(&Plasma::Corona::screenGeometry)>::type;
