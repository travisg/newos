#include <kernel.h>
#include <devs.h>

#include <con.h>

int devs_init(kernel_args *ka)
{

	console_dev_init(ka);

	return 0;
}
