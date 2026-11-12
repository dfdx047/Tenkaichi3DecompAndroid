#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_TIME
#include "../../third_party/miniz/miniz.h"
#include <assert.h>
#include <string.h>
int main(int argc,char **argv){
    mz_zip_archive z={0};size_t size=0;void *data;mz_uint i,dirs=0;
    assert(argc==2&&mz_zip_reader_init_file(&z,argv[1],0));
    assert(mz_zip_reader_get_num_files(&z)==3);
    for(i=0;i<3;++i){mz_zip_archive_file_stat st={0};char name[128]={0};
        assert(mz_zip_reader_file_stat(&z,i,&st));
        assert(mz_zip_reader_get_filename(&z,i,name,sizeof(name))>0);
        if(st.m_is_directory){++dirs;assert(st.m_uncomp_size==0);assert(!strncmp(name,"SLUS-21978/",11));}
    }
    assert(dirs==2);
    data=mz_zip_reader_extract_to_heap(&z,0,&size,0);
    assert(data&&size==12&&!memcmp(data,"BT4 ZIP64 OK",12));
    mz_free(data);mz_zip_reader_end(&z);return 0;
}
