#include "handle_reference.h"
#include "kernel.h"

void HandleReference::destroy() noexcept
{
	Kernel::GetHandleStorage().removeRef(m_id);
}
