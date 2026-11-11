#include <assert.h>

#include "m4_refresh_policy.h"

int main(void) {
    assert(m4_refresh_policy_from_project_mode(MODE_DU) == M4_REFRESH_POLICY_FAST);
    assert(m4_refresh_policy_from_project_mode(MODE_GL16) == M4_REFRESH_POLICY_HALF);
    assert(m4_refresh_policy_from_project_mode(MODE_GC16) == M4_REFRESH_POLICY_FULL);
    assert(m4_refresh_policy_from_project_mode((enum EpdDrawMode)0) == M4_REFRESH_POLICY_HALF);
    assert(m4_refresh_policy_should_present(M4_REFRESH_POLICY_FAST, M4_REFRESH_POLICY_HALF, true, true));
    assert(m4_refresh_policy_should_present(M4_REFRESH_POLICY_HALF, M4_REFRESH_POLICY_FULL, true, true));
    assert(!m4_refresh_policy_should_present(M4_REFRESH_POLICY_FULL, M4_REFRESH_POLICY_HALF, true, true));
    assert(!m4_refresh_policy_should_present(M4_REFRESH_POLICY_HALF, M4_REFRESH_POLICY_FAST, true, true));
    assert(m4_refresh_policy_should_present(M4_REFRESH_POLICY_FULL, M4_REFRESH_POLICY_FULL, true, true));
    assert(m4_refresh_policy_should_present(M4_REFRESH_POLICY_FULL, M4_REFRESH_POLICY_FAST, true, false));
    assert(m4_refresh_policy_resolve(M4_REFRESH_POLICY_FAST, true) == M4_REFRESH_POLICY_FULL);
    assert(m4_refresh_policy_resolve(M4_REFRESH_POLICY_HALF, false) == M4_REFRESH_POLICY_HALF);
    return 0;
}
