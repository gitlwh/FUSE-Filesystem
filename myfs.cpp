/**
 *  This code is based on an example file system from
 *    http://www.prism.uvsq.fr/~ode/in115/system.html
 *
 *  That site is now unreachable, but the presentation that accompanied that
 *  code is now on CourseSite.
 *
 *  Another useful tutorial is at
 *    http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/
 *
 *  Notes:
 *
 *  Fuse is not quite POSIX, but the function names usually have a good
 *  definition via 'man 3'
 *
 *  Missing functions and features:
 *
 *    Directories are not supported:
 *      mkdir, rmdir, opendir, releasedir, fsyncdir
 *
 *    Links are not supported:
 *      readlink, symlink, link
 *
 *    Permission support is limited:
 *      chmod, chown
 *
 *    Other unimplemented functions:
 *      statfs, flush, release, fsync, access, create, ftruncate, fgetattr
 *      (some of these are necessary!)
 *
 *    Files have fixed max length, which isn't a constant in the code
 *
 *    There is no persistence... if you unmount and remount, the files are
 *      gone!
 *
 *  Known bugs:
 *    Permission errors when moving out of and then back into the filesystem
 *
 *    Mountpoint has wrong permissions, due to a bug hidden in one of the
 *    functions
 *
 *    Root should be part of fuse_context
 *
 *    Not really working with file handles yet
 */

#include <fuse.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <vector>
#include <string>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif
using namespace std;
struct myfs_data {
    FILE *logfile;
    FILE *mountfile;
};

int rmid(int* ids, int id);
int eraseblock(int block);
int erasefile(int file);
int erasedir(int dir);
int getnum(int* a);


//to output key value of system, for debug
int log(const char* info){

FILE * lf=(static_cast<myfs_data*>(fuse_get_context()->private_data))->logfile;
if(lf==NULL) return -1;
fwrite(info, strlen(info), 1,lf);
fflush(lf);
return 0;
}



int log2(const char* info){

FILE * lf=(static_cast<myfs_data*>(fuse_get_context()->private_data))->logfile;
if(lf==NULL) return -1;
fwrite(info, strlen(info), 1,lf);
fwrite("\n",1, 1,lf);
fflush(lf);
return 0;
}


int log3(const string info){

FILE * lf=(static_cast<myfs_data*>(fuse_get_context()->private_data))->logfile;
if(lf==NULL) return -1;
fprintf(lf,"%s", info.c_str());
fflush(lf);
return 0;
}

//structure of block
struct block_t
{
	char data[65536];
	int next;
//	int last;
	int id;
	int valid;
};
block_t allblocks[50];

/**
 *  Helper function for managing permissions
 *
 *  [mfs] this might not always be doing the right thing ;)
 */
mode_t my_rights(struct stat* stbuf, uid_t uid, gid_t gid)
{char note[20]="in myright\n";
log(note);
    mode_t rights = 0;
    rights |= stbuf->st_mode & 7;
    if (stbuf->st_uid == uid)
        rights |= (stbuf->st_mode >> 6) & 7;
    if (stbuf->st_gid == gid)
        rights |= (stbuf->st_mode >> 3) & 7;
    return rights;
}
/*

 *  Describe a file
*/
struct file_t
{ int id;
    struct stat stbuf;
    int blockid;
    char path[50];
    int valid;
    int fatherdir;
};
file_t allfiles[20];



/**
 *  Describe a directory
 */
struct dir_t
{ int filesid[20];
int dirsid[20];
int id;
int fatherdir;
int valid;
	char path[50];
	struct stat stbuf;
};
dir_t alldirs[20];


//output the condition of all in the system, for debug

int showchildren(int i){
int x=getnum(alldirs[i].filesid);
int y=getnum(alldirs[i].dirsid);
if(x==0)log("no file inside ");
if(y==0)log2("no dir inside ");
for(int a=0;a<x;a++){
log("the num and path of inside file is ");
log(to_string(alldirs[i].filesid[a]).c_str());
log("and");
log2(allfiles[alldirs[i].filesid[a]].path);}
for(int a=0;a<y;a++){
log("the num and path of inside dir is ");
log(to_string(alldirs[i].dirsid[a]).c_str());
log("and");
log2(alldirs[alldirs[i].dirsid[a]].path);}



}


int show(){
log2("showing the file");
for(int i=0;i<20;i++){
if(allfiles[i].valid==1){
log("the ");log(to_string(i).c_str());log("file is");
log2(allfiles[i].path);
log("its father is ");log2(to_string(allfiles[i].fatherdir).c_str());
}
}
log2("showing the dir");
for(int i=0;i<20;i++){
/*if(i==1){
log("this case, dir1 seems disappaer, in face its path is ");log(alldirs[i].path);log("\nits valid bit is ");log2(to_string(alldirs[i].valid).c_str());
}*/
if(alldirs[i].valid==1){
log("the ");log(to_string(i).c_str());log("dir is");
log2(alldirs[i].path);
log("its father is ");log2(to_string(alldirs[i].fatherdir).c_str());
log2("its children are ");
showchildren(i);

}
}
return 0;
}

 
int show2(){
printf("showing the file\n");
for(int i=0;i<20;i++){
if(allfiles[i].valid==1)
printf("the %d file is %s\n",i,allfiles[i].path);
}
printf("showing the dir\n");
for(int i=0;i<20;i++){
if(alldirs[i].valid==1)
printf("the %d dir is %s\n",i,alldirs[i].path);
}
return 0;
}



/**
 *  The root directory of our filesystem
 */
dir_t &root=alldirs[0];

//get fileid from a path

int getfile(const char* path){
char note[20]="in getfile\n";
log(note);
int i;
for(i=0;i<20;i++){
if(strcmp(path,allfiles[i].path)==0)
return i;

}
return -1;
}

//get dirid from a path

int getdir(const char* path){
char note[20]="in getdir\n";
log(note);
int i;
for(i=0;i<20;i++){
if(strcmp(path,alldirs[i].path)==0)
return i;

}
return -1;
}
/**
 *  Get a new wanted element
 */	

int findnewblock(){
char note[20]="in findnewblock\n";
log(note);
for(int i=0;i<50;i++){
if(allblocks[i].valid==0)
return i;
}
return -1;
}

int findnewfile(){
char note[20]="in findnewfilek\n";
log(note);
for(int i=0;i<20;i++){
if(allfiles[i].valid==0)
return i;
}
return -1;
}

int findnewdir(){
char note[20]="in findnewdir\n";
log(note);
for(int i=0;i<20;i++){
if(alldirs[i].valid==0)
return i;
}
return -1;
}

//clear useless block

int clearnextblock(int thisblock){
char note[20]="in clearnextblock\n";
log(note);
int blocknum=allblocks[thisblock].next;
if(blocknum!=-1){
clearnextblock(blocknum);
}
allblocks[blocknum].valid=0;
allblocks[thisblock].next=-1;
return 0;
}

//get the (next)th block of one file

int getnextblock(int nowblock, int next){
char note[20]="in getnextblock\n";
log(note);
if(next==0)return nowblock;
if(allblocks[nowblock].next==-1)
return -1;
if(next>1)return getnextblock(nowblock,next-1);
else return allblocks[nowblock].next;
}



//see if one path is in another path

int ifindir(const char* inpath, const char* dirpath){
char note[20]="in ifindir\n";
log(note);
int i;
if(getdir(dirpath)==0){
i=1;
	while(inpath[i]!='\0'){
	if(inpath[i]=='/')return 0;
	i++;	
	}
return 1;

}
if(strncmp(inpath,dirpath,strlen(dirpath))==0)
{
int i=strlen(dirpath)+2;
while(inpath[i]!='\0'){
if(inpath[i]=='/')return 0;
i++;
}
return 1;

}
return 0;

}

//get the up directory of one element

int getfather(const char* path){
char note[20]="in getfather\n";
log(note);
int i;
for(i=0;i<20;i++){
if(alldirs[i].valid==0)continue;
if(ifindir(path,alldirs[i].path))
return i;
}
return -1;


}


//get the attribution

int myfs_getattr(const char* path, struct stat* stbuf)
{
char note[20]="in getattr\n";
log(note);

if((getfile(path)==-1)&&(getdir(path)==-1)) {log("your failed in getting attribute! Now the path is ");
//log2(path);log2("and your document element is");
return -ENOENT;}
else if(getfile(path)!=-1)
memcpy(stbuf, &allfiles[getfile(path)].stbuf, sizeof(struct stat));
else if(getdir(path)!=-1){

log("I have success find the attribute of path ");log2(path);
memcpy(stbuf, &alldirs[getdir(path)].stbuf, sizeof(struct stat));}
    return 0;

}

/**
 *  Man 3 truncate suggests this is not a conforming implementation
 */
int myfs_truncate(const char* path, off_t size)
{
char note[20]="in truncate\n";
log(note);
    int file = getfile(path);


    fuse_context* context = fuse_get_context();
    mode_t rights = my_rights(&(allfiles[file].stbuf), context->uid, context->gid);
    if (!(rights & 2))
        return -EACCES;
if(size>65536){
int num=size/65536;
int finalblock=getnextblock(allfiles[file].blockid,num);
clearnextblock(finalblock);}
    allfiles[file].stbuf.st_size = size;
    return 0;

}

/**
 *  Rename: not safe once we have more folders
 */
int myfs_rename(const char* from, const char* to)
{
char note[20]="in rename\n";
log(note);
fuse_context* context = fuse_get_context();

if(getfile(from)!=-1){

	if (allfiles[getfile(from)].stbuf.st_uid != context->uid)
		return -EACCES;

int father = getfather(from);
rmid(alldirs[father].filesid,getfile(from));
int newfather = getfather(to);

for(int i=0;i<20;i++){
if(alldirs[newfather].filesid[i]==-1){
alldirs[newfather].filesid[i]=getfile(from);
break;
}
}
strcpy(allfiles[getfile(from)].path, to);
return 0;
}

if(getdir(from)!=-1){
	if (alldirs[getdir(from)].stbuf.st_uid != context->uid)
		return -EACCES;
int father = getfather(from);
rmid(alldirs[father].dirsid,getdir(from));
int newfather = getfather(to);

for(int i=0;i<20;i++){
if(alldirs[newfather].dirsid[i]==-1){
alldirs[newfather].dirsid[i]=getdir(from);
break;
}
}
strcpy(alldirs[getdir(from)].path, to);
return 0;
}
return -ENOENT;
}


/**
 *  Example of how to set time
 */
int myfs_utime(const char* path, struct utimbuf* buf)
{char note[20]="in utime\n";
log(note);
    fuse_context* context;
    context = fuse_get_context();
    if (buf != 0) {
        if (context->uid != 0 && allfiles[getfile(path)].stbuf.st_uid != context->uid)
            return -EPERM;
        allfiles[getfile(path)].stbuf.st_atime = allfiles[getfile(path)].stbuf.st_mtime = time(0);
    }
    else {
        mode_t rights = my_rights(&allfiles[getfile(path)].stbuf, context->uid, context->gid);
        if (context->uid != 0 && allfiles[getfile(path)].stbuf.st_uid != context->uid
            && !(rights & 2))
        {
            return -EACCES;
        }
        allfiles[getfile(path)].stbuf.st_atime = buf->actime;
        allfiles[getfile(path)].stbuf.st_mtime = buf->modtime;
    }
    return 0;
}




/**
 *  Write to a file
 */
int myfs_write(const char* path, const char* buf, size_t size,
               off_t offset, fuse_file_info* fi)
{
char note[20]="in write\n";
log(note);
int file = getfile(path);


    int diff = offset+size-allfiles[file].stbuf.st_size;
int num=offset/65536;
int block_offset=offset%65536;
int i;
int nowblock=allfiles[file].blockid;
nowblock=getnextblock(nowblock, num);
clearnextblock(nowblock);
if (block_offset+size <= 65536)
    memcpy(&(allblocks[nowblock].data[block_offset]), buf, size);
else
{
memcpy(&(allblocks[nowblock].data[block_offset]), buf, 65536-block_offset);
int newblocknum = (block_offset+size)/65536;
for(i=0;i<newblocknum;i++){
	allblocks[nowblock].next=findnewblock();
	allblocks[findnewblock()].next=-1;
//	allblocks[findnewblock()].last = allblocks[nowblock].id;
	
if(newblocknum-i==1){
memcpy(&(allblocks[findnewblock()].data[0]), buf+65536-block_offset+i*65536, (block_offset+size)%65536);
}else
memcpy(&(allblocks[findnewblock()].data[0]), buf+65536-block_offset+i*65536, 65536);

	nowblock=allblocks[nowblock].next;
	allblocks[findnewblock()].valid=1;
}


}

        allfiles[file].stbuf.st_size = offset+diff;
        allfiles[file].stbuf.st_blocks = (offset+diff)/65536+1;
    return size;
}



/**
 *  Read from a file
 */
int myfs_read(const char* path, char* buf, size_t size,
              off_t offset, fuse_file_info* fi)
{char note[20]="in read\n";
log(note);
int file = getfile(path);
int num=offset/65536;
int block_offset=offset%65536;
int nowblock=getnextblock(allfiles[file].blockid,num);
if(block_offset+size<=65536){
memcpy(buf, &(allblocks[nowblock].data[offset]), size);

}else{
int outnum=(block_offset+size)/65536;
for(int i=0;i<outnum;i++){
nowblock=allblocks[nowblock].next;
if(outnum-i==1){
memcpy(buf+65536-block_offset+65536*i, &(allblocks[nowblock].data[0]), size+block_offset-65536*i-65536);
}else{
memcpy(buf+65536-block_offset+65536*i, &(allblocks[nowblock].data[0]), 65536);
}
}
}
    return size;
}





/**
 *  Make a new entry in the filesystem
 */
int myfs_mknod(const char* path, mode_t mode, dev_t dev)
{
char note[20]="in mknod\n";
log(note);
    fuse_context* context = fuse_get_context();
    int newfile=findnewfile();
    strcpy(allfiles[newfile].path, path);
    memset(&allfiles[newfile].stbuf, 0, sizeof(struct stat));
    allfiles[newfile].stbuf.st_mode = mode;
    allfiles[newfile].stbuf.st_dev = dev;
    allfiles[newfile].stbuf.st_nlink = 1;
    allfiles[newfile].stbuf.st_atime = allfiles[newfile].stbuf.st_mtime =
        allfiles[newfile].stbuf.st_ctime = time(0);
    allfiles[newfile].stbuf.st_uid = context->uid;
    allfiles[newfile].stbuf.st_gid = context->gid;
    allfiles[newfile].stbuf.st_blksize = 65536;
allfiles[newfile].stbuf.st_size=0;
allfiles[newfile].valid=1;
allfiles[newfile].blockid=findnewblock();
allfiles[newfile].fatherdir=getfather(path);
for(int i=0;i<20;i++){

if(alldirs[allfiles[newfile].fatherdir].dirsid[i]==-1){
alldirs[allfiles[newfile].fatherdir].dirsid[i]=newfile;break;
}

}
	allblocks[findnewblock()].next=-1;
	allblocks[findnewblock()].valid=1;



    return 0;
}

/*

    int blockid;
    char[50] path;
    int valid;
    int fatherdir;


*/

/**
 *  make one new directory
 */



int myfs_mkdir(const char *path, mode_t mode)
{
char note[20]="in mkdir: path is";
log(note);
log2(path);
	fuse_context* context = fuse_get_context();

	int newdir = findnewdir();
	strcpy(alldirs[newdir].path ,path);
	memset(&alldirs[newdir].stbuf, 0, sizeof(struct stat));
	alldirs[newdir].stbuf.st_mode = S_IFDIR | 0755;
	alldirs[newdir].stbuf.st_nlink = 2;
	alldirs[newdir].stbuf.st_atime = alldirs[newdir].stbuf.st_mtime =
		alldirs[newdir].stbuf.st_ctime = time(0);
	alldirs[newdir].stbuf.st_uid = context->uid;
	alldirs[newdir].stbuf.st_gid = context->gid;
	alldirs[newdir].valid=1;
	alldirs[newdir].fatherdir=getfather(path);
for(int i=0;i<20;i++){

if(alldirs[alldirs[newdir].fatherdir].dirsid[i]==-1){
alldirs[alldirs[newdir].fatherdir].dirsid[i]=newdir;break;
}

}
	return 0;
}

/*

int filesid[20];
int dirsid[20];
int id;
int fatherdir;
int valid
	char[50] path;
	struct stat stbuf;


*/


//remove one id from one id group

int rmid(int* ids, int id){
char note[20]="in removeid\n";
log(note);
int i;
if(ids[0]==-1) return 0;
for(i=0;i<20;i++){
if(ids[i]==-1) return 0;
if(ids[i]==id) break;
}
for(;i<20;i++){
if(ids[i]==-1) return 0; 
ids[i]=ids[i+1];

}
return 0;
}

//as the name

int eraseblock(int block){
char note[20]="in eraseblock\n";
log(note);
clearnextblock(block);
allblocks[block].valid=0;
return 0;
}

int erasefile(int file){
char note[20]="in erasefile\n";
log(note);
rmid(alldirs[allfiles[file].fatherdir].filesid,file);
eraseblock(allfiles[file].blockid);
allfiles[file].valid=0;
return 0;
}

int erasedir(int dir){
char note[20]="in erasedir\n";
log(note);
int i=getnum(alldirs[dir].filesid);
int j=getnum(alldirs[dir].dirsid);
for(int a=0;a<i;a++){
erasefile(alldirs[dir].filesid[a]);
}
for(int a=0;a<j;a++){
erasedir(alldirs[dir].dirsid[a]);
}
rmid(alldirs[alldirs[dir].fatherdir].dirsid,dir);
alldirs[dir].valid=0;
return 0;
}

/*

int id
    struct stat stbuf;
    int blockid;
    char[50] path;
    int valid;
    int fatherdir;
*/

//delete one directory
int myfs_rmdir(const char*path)
{char note[20]="in rmdir\n";
log(note);
fuse_context* context = fuse_get_context();
	if (alldirs[getdir(path)].stbuf.st_uid != context->uid)
		return -EACCES;
	int dir=getdir(path);
	erasedir(dir);
	return 0;
}

//get the number of element in one array
int getnum(int* a){
int i;
for(i=0;i<20;i++){
if(a[i]==-1)break;
}
return i;
}

//get the content of one dir
int myfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                 off_t offset, fuse_file_info* fi)
{
char note[20]="in readdir\n";
log(note);

int dir = getdir(path);

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    // the only directory is root... otherwise we may need to use fi->fh
    //
    // NB: using C++0x type inference for iterators... w00t!
int i=getnum(alldirs[dir].filesid);
int j=getnum(alldirs[dir].dirsid);

if(i>0)
for(int a=0;a<i;a++)
        if(filler(buf, allfiles[alldirs[dir].filesid[a]].path, &allfiles[alldirs[dir].filesid[a]].stbuf, 0)!=0)log("aaaaaaaaaaaaaaaaaa");
if(j>0)
for(int a=0;a<j;a++)
        if(filler(buf, alldirs[alldirs[dir].dirsid[a]].path, &alldirs[alldirs[dir].dirsid[a]].stbuf, 0)!=0)log("aaaaaaaaaaaaaaaaaaaaa");
  
    return 0;
}

/**
 *  Open a file
 */
int myfs_open(const char* path, fuse_file_info* fi)
{char note[20]="in open\n";
log(note);
    fuse_context* context;
int file= getfile(path);


    context = fuse_get_context();
    if (context->uid != 0) {
        mode_t rights = my_rights(&allfiles[file].stbuf, context->uid, context->gid);
        if ((fi->flags & O_RDONLY && !(rights & 4)) ||
            (fi->flags & O_WRONLY && !(rights & 2)) ||
            (fi->flags & O_RDWR && ! ((rights&4)&&(rights&2))))
            return -EACCES;
    }

    return 0;
}
//change mode

int myfs_chmod(const char* path, mode_t mode)
{char note[20]="in chmod\n";
log(note);
	int file = getfile(path);
	allfiles[file].stbuf.st_mode = mode;
	return 0;
}
//change owner
int myfs_chown(const char* path, uid_t uid, gid_t gid)
{char note[20]="in chown\n";
log(note);
	int file = getfile(path);
	allfiles[file].stbuf.st_uid = uid;
	allfiles[file].stbuf.st_gid = gid;
	return 0;
}




/**
 *  For debugging, to get you started with figuring out why root permissions
 *  are funny
 */
void info()
{
    uid_t a = getuid(), b = geteuid(), c = getgid(), d = getegid();
    cerr << "uid, euid, gid, egid = "
         << a << ", "
         << b << ", "
         << c << ", "
         << d << endl;
}

/**
 *  Initialization code... this isn't quite correct (have fun :)
 *
 *  Note: we need to set up the root object, so that we can stat it later
 */
void* myfs_init(fuse_conn_info* conn)
{char note[20]="in init\n";
log(note);
    info();
log2("before initial");
show();
    fuse_context* context = fuse_get_context();
log("at this time, root is no. ");
log2(to_string(root.id).c_str());
root.id=0;
root.fatherdir=-1;
strcpy(root.path,"/");
  memset(&root.stbuf, 0, sizeof(struct stat));
  root.stbuf.st_mode = S_IFDIR | 0755;
  root.stbuf.st_nlink = 2;
    root.stbuf.st_uid = context->uid;
    root.stbuf.st_gid = context->gid;
    root.stbuf.st_ctime = root.stbuf.st_mtime = root.stbuf.st_atime = time(0);
root.valid=1;
log2("after initial");
show();
    return context->private_data;
}



/**
 *  This won't be correct once hard links are supported
 */
int myfs_unlink(const char* path)
{char note[20]="in unlink\n";
log(note);

	int file=getfile(path);

	if (file == -1)
		return -ENOENT;
	fuse_context* context = fuse_get_context();
	if (allfiles[file].stbuf.st_uid != context->uid)
		return -EACCES;
	erasefile(file);
	return 0;
}

/*
int erasefile(int file){
rmid(alldirs[allfiles[file].fatherdir].filesid,file);
eraseblock(allfiles[file].blockid);
allfiles[file].valid=0;
return 0;
}

*//*

int myfs_opendir(const char* path, struct fuse_file_info* fi)
{char note[20]="in opendir\n";
log(note);
	//log("\nopendir is called\n");
	return 0;
}

int myfs_release(const char*path, struct fuse_file_info *fi)
{	char note[20]="in release\n";
log(note);
	//log_msg("\nrelease is called\n");
	return 0;
}


int myfs_releasedir(const char* path, struct fuse_file_info* fi)
{char note[20]="in releasedir\n";
log(note);
	//log_msg("\nreleasedir is called\n");
	return 0;
}

int myfs_fsyncdir(const char* path, int datasync, struct fuse_file_info* fi)
{char note[20]="in fsyncdir\n";
log(note);
	//log_msg("\nfsyncdir is called\n");
	return 0;
}
*/

/*
int filesid[20];
int dirsid[20];
int id;
int fatherdir;
int valid
	char[50] path;
	struct stat stbuf;

*/

/**
 *  When we unmount a filesystem, this gets called.
 */
void myfs_destroy (void* state)
{char note[20]="in destroy\n";
log(note);
log("when destroying, the files and dirs are as below:");
show();
FILE* mf=(static_cast<myfs_data*>(fuse_get_context()->private_data))->mountfile;
fseek(mf,0,SEEK_SET);
for(int j=0;j<20;j++){
fwrite(&alldirs[j], sizeof(dir_t), 1,mf);
}
for(int j=0;j<20;j++){
fwrite(&allfiles[j], sizeof(file_t), 1,mf);
}
for(int j=0;j<50;j++){
fwrite(&allblocks[j], sizeof(block_t), 1,mf);
}
fclose((static_cast<myfs_data*>(fuse_get_context()->private_data))->logfile);
fclose((static_cast<myfs_data*>(fuse_get_context()->private_data))->mountfile);

}

/**
 *  To configure our filesystem, we need to create a fuse_operations struct
 *  that has pointers to all of the functions that we have created.
 *  Unfortunately, when we do this in C++, we can't use easy struct initializer
 *  lists, so we need to make an initialization function.
 */
fuse_operations myfs_ops;
void pre_init()
{





  //  myfs_ops.opendir = myfs_opendir;
 //  myfs_ops.releasedir = myfs_releasedir;
 //  myfs_ops.fsyncdir = myfs_fsyncdir;

    myfs_ops.chmod = myfs_chmod;
    myfs_ops.chown = myfs_chown;

    myfs_ops.open = myfs_open;
 //   myfs_ops.release = myfs_release;
    myfs_ops.rmdir = myfs_rmdir;
    myfs_ops.mkdir = myfs_mkdir;
    myfs_ops.destroy = myfs_destroy;
    myfs_ops.getattr = myfs_getattr;
    myfs_ops.readdir = myfs_readdir;
    myfs_ops.open = myfs_open;
    myfs_ops.init = myfs_init;
    myfs_ops.read = myfs_read;
    myfs_ops.mknod = myfs_mknod;	
    myfs_ops.truncate = myfs_truncate;
    myfs_ops.write = myfs_write;
    myfs_ops.utime = myfs_utime;
//    myfs_ops.unlink = myfs_unlink;
    myfs_ops.rename = myfs_rename;
    info();
}

//get all information from disk


int myfs_restore(FILE* mf){
printf("restoring...\n");
for(int i=0; i<20;i++){
fread(&alldirs[i], sizeof(dir_t),1,mf);
}
for(int i=0;i<20;i++){
fread(&allfiles[i], sizeof(file_t),1,mf);
}
for(int i=0;i<50;i++){
fread(&allblocks[i], sizeof(block_t),1,mf);
}
printf("after restoring, the files and dirs are as below:\n");
show2();
return 0;

}

//when first time run this system, run this

int initial(){
printf("initialing...\n");
for(int i=0; i<20;i++)
{
alldirs[i].valid=0;
alldirs[i].id=i;
for(int j=0;j<20;j++){
alldirs[i].dirsid[j]=-1;
alldirs[i].filesid[j]=-1;
}
}
for(int i=0;i<20;i++){
allfiles[i].valid=0;
allfiles[i].id=i;
}
for(int i=0;i<50;i++){
allblocks[i].valid=0;
allblocks[i].id=i;
allblocks[i].next=-1;
}
return 0;
};
/*
struct file_t
{ int id;
    struct stat stbuf;
    int blockid;
    char path[50];
    int valid;
    int fatherdir;
};

struct dir_t
{ int filesid[20];
int dirsid[20];
int id;
int fatherdir;
int valid;
	char path[50];
	struct stat stbuf;
};

/**
 *  Fuse main can do some initialization, but needs to call fuse_main
 *  eventually...
 */
int main(int argc, char* argv[])
{/*
    pre_init();
    return fuse_main(argc, argv, &myfs_ops, NULL);
*/

//new
printf("in main\n");

char *mount_file = argv[argc-1];
FILE* mf =fopen(mount_file,"rb+");
if(mf!=NULL){
printf("to restore...\n");
myfs_restore(mf);
}else{
printf("first time!\n");
initial();
mf =fopen(mount_file,"wb+");
fseek(mf,0,SEEK_SET);
}
FILE* lf =fopen("myfs_log","w+");
struct myfs_data* private_data = new myfs_data{lf,mf};
//char *z;
//z="abcd";
//e="efgh";
//printf("trying to write...\n");
//fprintf(lf,"%s\n", z);
//fwrite(z, sizeof(char)*4,1,lf);
//fflush(lf);
//fwrite(e, sizeof(char)*4, 1,lf);
//fread(e,sizeof(char)*4,1,lf);
//cout<<e<<endl;
//fseek(lf,0,SEEK_SET);



//char note[20]="in init\n";
//fwrite(note, sizeof(note), 1,lf);
//fwrite("\n", sizeof(char), 1,lf);
//fflush(lf);



pre_init();
argv[argc-1]="-s";
    return fuse_main(argc, argv, &myfs_ops, private_data);
//new

}
