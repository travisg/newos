/*
	rm: remove util.
	Parameters: Filenames
	exit      : 1 when deleting  one of the files failed
*/

#include <sys/syscalls.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RMS_OK          0
#define RMS_FILE_IS_DIR 1

static int do_delete(const char *name)
{
	struct file_stat stat;
	int err = sys_rstat(name,&stat);

	if(err<0) return err;

	if(stat.type == STREAM_TYPE_DIR) return RMS_FILE_IS_DIR;

	return  sys_unlink(name);
}

int main(int argc,char *argv[])
{
	int cnt;
	const char *name;
	int err;
	const char *err_text;
	int ret = 0;

	if(argc <= 1){

		printf("rm: Missing arguments\n");
		ret = 1;

	}

	for(cnt=1;cnt<argc;cnt++){
		name = argv[cnt];
		err = do_delete(name);

		if(err != 0){
			ret = 1;
			if(err == 1) {
				err_text = "File is a directory";
			} else {
				err_text = strerror(err);
			}
			printf("rm: %s \n",err_text);
		}
	}

	return ret;
}

