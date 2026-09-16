#include "textflag.h"

// isb: the pause of sgcl/detail/os.h spin_pause, for the backoff; Go's
// runtime uses the same instruction in its own spins (procyield)
TEXT ·isb(SB),NOSPLIT,$0-0
	ISB $15
	RET
