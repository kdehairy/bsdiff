//
// Created by kdehairy on 9/19/15.
//

#include <string.h>
#include "test_diff.h"
#include "test_patch.h"

int main(int argc, char *const argv[]) {
    const char *command = argv[1];
    const char *base = argv[2];
    const char *variant = argv[3];
    const char *patch = argv[4];

    if (memcmp(command, "diff", 4) == 0) {
        testDiff(base, variant, patch);
    } else if (memcmp(command, "patch", 5) == 0) {
        testPatch(base, variant, patch);
    }


}