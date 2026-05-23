#pragma once

namespace ceramic {
int parachute(int (*mainfn)(int, char **, char const *const *), int argc,
              char **argv, char const *const *envp);
}
