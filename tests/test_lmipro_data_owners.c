#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef void *mount_t;
typedef void *vfs_context_t;
struct node {mount_t mount;unsigned uid,gid,mode,supported;int type;};
typedef struct node *vnode_t;
#define NULLVP ((vnode_t)0)
#define VDIR 2
struct vnode_attr {unsigned va_uid,va_gid,va_mode,va_supported;};
#define B_va_uid 1U
#define B_va_gid 2U
#define B_va_mode 4U
#define VATTR_INIT(p) memset((p),0,sizeof(*(p)))
#define VATTR_WANTED(p,a) ((void)(p))
#define VATTR_SET(p,a,v) ((p)->a=(v))
#define VATTR_IS_SUPPORTED(p,a) ((p)->va_supported&B_##a)
#define VATTR_IS(p,a,v) (VATTR_IS_SUPPORTED(p,a) && (p)->a==(v))
static struct node nodes[3];
static unsigned actor_uid,puts_count,set_count,lookup_count,get_count;
static int lookup_error,set_error,get_error,drop_update;
static vfs_context_t vfs_context_current(void){return (void *)1;}
static void *vfs_context_ucred(vfs_context_t ctx){return ctx;}
static unsigned kauth_cred_getuid(void *cred){(void)cred;return actor_uid;}
static int vnode_lookup(const char *name,int flags,vnode_t *out,vfs_context_t ctx){
 (void)flags;(void)ctx;++lookup_count;if(lookup_error)return lookup_error;
 unsigned i=strstr(name,"/tmp")?0:strstr(name,"/Documents")?1:2;*out=&nodes[i];return 0;
}
static mount_t vnode_mount(vnode_t n){return n->mount;}
static int vnode_vtype(vnode_t n){return n->type;}
static int vnode_getattr(vnode_t n,struct vnode_attr *a,vfs_context_t ctx){
 (void)ctx;++get_count;if(get_error)return get_error;
 a->va_uid=n->uid;a->va_gid=n->gid;a->va_mode=n->mode;a->va_supported=n->supported;return 0;
}
static int vnode_setattr(vnode_t n,struct vnode_attr *a,vfs_context_t ctx){
 (void)ctx;++set_count;if(set_error)return set_error;
 if(!drop_update){n->uid=a->va_uid;n->gid=a->va_gid;}return 0;
}
static int vnode_put(vnode_t n){assert(n);++puts_count;return 0;}
#include "owner-helper.c"
static void reset(void){
 actor_uid=0;puts_count=set_count=lookup_count=get_count=0;
 lookup_error=set_error=get_error=drop_update=0;
 lmipro_data_owner_stage=lmipro_data_owner_status=-1;
 for(unsigned i=0;i<3;++i)nodes[i]=(struct node){(void *)1,0,501,0755,7,VDIR};
}
int main(void){
 reset();assert(!lmipro_data_mobile_owners((void *)1));
 assert(lmipro_data_owner_stage==4 && !lmipro_data_owner_status && set_count==3 && puts_count==3);
 for(unsigned i=0;i<3;++i)assert(nodes[i].uid==501 && nodes[i].gid==501 && nodes[i].mode==0755);
 assert(!lmipro_data_mobile_owners((void *)1));assert(set_count==3 && puts_count==6);
 reset();actor_uid=501;assert(lmipro_data_mobile_owners((void *)1)==EPERM && !lookup_count);
 reset();assert(lmipro_data_mobile_owners(0)==EPERM && !lookup_count);
 reset();nodes[0].mount=(void *)2;assert(lmipro_data_mobile_owners((void *)1)==EINVAL && puts_count==1 && !set_count);
 reset();nodes[0].type=5;assert(lmipro_data_mobile_owners((void *)1)==EINVAL && puts_count==1 && !set_count);
 reset();nodes[0].mode=0700;assert(lmipro_data_mobile_owners((void *)1)==EINVAL && !set_count);
 reset();nodes[0].uid=502;assert(lmipro_data_mobile_owners((void *)1)==EINVAL && !set_count);
 reset();nodes[0].gid=0;assert(lmipro_data_mobile_owners((void *)1)==EINVAL && !set_count);
 reset();nodes[0].supported=3;assert(lmipro_data_mobile_owners((void *)1)==ENOTSUP && puts_count==1 && !set_count);
 reset();lookup_error=ENOENT;assert(lmipro_data_mobile_owners((void *)1)==ENOENT && !puts_count);
 reset();get_error=EIO;assert(lmipro_data_mobile_owners((void *)1)==EIO && puts_count==1 && !set_count);
 reset();set_error=EACCES;assert(lmipro_data_mobile_owners((void *)1)==EACCES && puts_count==1 && lmipro_data_owner_stage==1);
 reset();drop_update=1;assert(lmipro_data_mobile_owners((void *)1)==EIO && puts_count==1 && lmipro_data_owner_stage==1);
 puts("PASS: exact compiled provisioning body; fixed scope, owner/mode policy, readback, references and original errors");
}
