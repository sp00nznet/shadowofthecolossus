/* hle_overrides.c -- SPURS 2.0, Job-Queue Port2 and avconf imports this port
 * serves itself.
 *
 * This is the wall the first boot hit (README "First boot", PROGRESS 2026-09-05):
 * every unresolved NID in the run belongs to one of three surfaces the runtime
 * does not implement for this title. All the sister ports run on SPURS 1.0;
 * NPUA-80677 is the first title in the family on the SPURS 2.0 surface, so
 * `cellSpursInitializeWithAttribute2` fakes CELL_OK, no SPURS instance ever
 * registers, `CreateTaskset REJECT unregistered spurs=...` follows, the engine's
 * FIOS worker taskset never exists, and the title parks on
 * `wait for invalid cond 'fios worker cond'` without ever opening nico.psarc.
 *
 * WHAT THIS FILE IS: a bring-up shim, registered the way twistedmetal-psn's
 * src/hle_overrides.c registers its own imports -- a constructor that calls
 * ps3_hle_register / ps3_hle_register_ctx directly, for NIDs that upstream
 * ps3recomp has no handler for (none of the 15 below collide with the generated
 * gen/ppu_hle_nids.cpp table; first match would win anyway, and the constructor
 * runs before ppu_hle_init).
 *
 * SOURCES FOR EVERYTHING HERE (no GPL code copied; RPCS3 is a behavioural
 * reference only, per ps3recomp's README):
 *   - prototypes ..... the reference Cell SDK headers mirrored in
 *                       FirebirdTA01/PS3DK (sdk/include/cell/spurs/ headers):
 *                       types.h, task.h, task_types.h, job_queue.h,
 *                       job_queue_port2.h -- exact C ABIs for all 15 entry points
 *   - struct layouts . task_types.h (TasksetAttribute2 = 512 B align 8,
 *                       TaskAttribute2 = 256 B align 16, TaskLsPattern = 16 B),
 *                       cross-checked against RPCS3's reverse-engineered
 *                       cellSpurs.h, which agrees on every offset read here
 *   - NIDs ........... every NID below is recomputed with ps3recomp's own
 *                       nid_database.compute_nid(name) and matches the NID the
 *                       first boot reported unresolved (README NID table)
 *   - behaviour ...... RPCS3's cellSpurs.cpp / cellAvconfExt.cpp for the
 *                       semantics of the *2 initializers, and ps3recomp's own
 *                       libs/spurs for what the forwarded 1.0 path expects
 *
 * THE SHAPE OF THE FIX: forward each 2.0 call into the 1.0 implementation that
 * already works in this runtime, translating structs at the boundary:
 *
 *   InitializeWithAttribute2 -> cellSpursInitializeWithAttribute + zero the
 *                               extended half of the 8 KB CellSpurs2 instance
 *   CreateTaskset2           -> cellSpursCreateTasksetWithAttribute with a
 *                               synthesized v1 attribute whose tasksetSize is
 *                               0x2900 (the v2 size that path already accepts)
 *   CreateTask2              -> cellSpursCreateTask, context/lsPattern taken
 *                               from the v2 task attribute
 *   _CreateJobQueue          -> cellSpursCreateJobQueue (ctx handler: 10 args,
 *                               so arg9/arg10 come off the guest stack at SP+112)
 *   Port2PushJobBody         -> cellSpursJobQueuePush through the port's first
 *                               word (Port2Create aliases port <- queue slot)
 *   Port2Sync                -> CELL_OK: the Jq HLE completes jobs on push
 *   six Jq AttributeSet*     -> write the values into the (opaque, 512 B) attr
 *   cellVideoOutGetScreenSize-> mirror RPCS3: no stereo configured -> VALUE_IS_NOT_SET
 *
 * WHAT THIS DOES NOT FIX (honest limits, see PROGRESS.md):
 *   - It is a shim, not an implementation. The real SPURS 2.0 surface belongs
 *     upstream in ps3recomp (libs/spurs), as PROGRESS.md says; this file exists
 *     to unblock the boot and test the hypothesis that FIOS then opens the PSARC.
 *   - cellSpursCreateTasksetWithAttribute currently drops argTaskset (it calls
 *     spurs_create_taskset(..., 0, NULL, 0, size)), so the v2 taskset's args
 *     field is written as 0. The value is carried in the synthesized attribute
 *     anyway, so an upstream fix needs no change here.
 *   - Job-queue pushes still complete synchronously with no SPU execution --
 *     that is the runtime's pre-existing cellSpursJq design, not something
 *     this file adds.
 *   - sys_ppu_thread_once (0xA3E3BE68) is deliberately NOT registered here:
 *     upstream ps3recomp already implements it (libs/system/sysPrxForUser.c);
 *     it appears in the first-boot list because that run predates it. Re-run
 *     tools/relift.sh to regenerate gen/ppu_hle_nids.cpp and it resolves.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ppu_context.h"   /* ppu_context -- ctx handler arg access */
#include "ppu_memory.h"    /* vm_base, GUEST_PTR, vm_read32/64, vm_write32/64 */
#include "cellSpurs.h"     /* upstream 1.0 entry points we forward into */
#include "cellSpursJq.h"   /* upstream job-queue entry points */

extern void ps3_hle_register(uint32_t nid, const char* name, void* handler);
extern void ps3_hle_register_ctx(uint32_t nid, const char* name,
                                 void (*fn)(ppu_context*));

/* ---- guest struct offsets (BE; always accessed via vm_read/vm_write) ---- */

/* CellSpursTasksetAttribute2 -- SDK task_types.h: 512 B, align 8.
 * revision@0, name@4 (PRXPTR, 4 B), args@8, priority[8]@0x10,
 * maxContention@0x18, enableClearLs@0x1C, taskNameBuffer@0x20 (4 B). */
enum {
    TSET2_REVISION    = 0x00,
    TSET2_ARGS        = 0x08,
    TSET2_PRIORITY    = 0x10,
    TSET2_MAXCONTE    = 0x18,
    TSET2_SIZE        = 512,
};

/* CellSpursTaskAttribute2 -- SDK task_types.h: 256 B, align 16.
 * revision@0, sizeContext@4, eaContext@8, lsPattern@0x10 (16 B), name@0x20. */
enum {
    TASK2_REVISION    = 0x00,
    TASK2_SIZECONTEXT = 0x04,
    TASK2_EACONTEXT   = 0x08,
    TASK2_LSPATTERN   = 0x10,
    TASK2_SIZE        = 256,
};

/* CellSpurs2 instance = 2 x CELL_SPURS_SIZE (types.h: same alignment, twice
 * the bytes); ps3recomp's 1.0 init clears only the first 4096 (see the
 * SPURS_INST_SIZE note in libs/spurs/cellSpurs.c). */
enum { SPURS_SIZE1 = 0x1000, SPURS_SIZE2 = 0x2000 };

/* Taskset sizes accepted by cellSpursCreateTasksetWithAttribute. */
enum { TASKSET2_SIZE = 0x2900 };

/* The synthesized v1 CellSpursTasksetAttribute we hand to
 * cellSpursCreateTasksetWithAttribute, parked in the taskset2 tail (the
 * 0x2180..0x2900 region of CellSpursTaskset2 is reserved bytes per RPCS3's
 * layout -- the create path reads the attribute BEFORE it memsets the
 * taskset, so the temp is consumed then wiped). Native/LITTLE-endian stores:
 * that is this runtime's convention for v1 taskset attributes (its own
 * Initialize writes them host-endian and its create reads them back
 * host-endian; the guest treats the attribute as opaque bytes). */
enum { V1ATTR_SIZE = 88, V1ATTR_OFF = TASKSET2_SIZE - V1ATTR_SIZE }; /* 0x28A8 */
enum { V1ATTR_REVISION = 0, V1ATTR_ARGS = 8, V1ATTR_PRIORITY = 16,
       V1ATTR_TASKSETSIZE = 24 };

/* CellSpursJobQueueAttribute in this runtime's host/LE view (libs/spurs/
 * cellSpursJq.h: maxJobs@0, maxGrab@4, priorities@8, flags@16, reserved@20).
 * The SDK type is an opaque 512-byte bag the game never reads directly -- the
 * only consumers are this runtime's own Initialize/Create, which speak
 * host-endian -- so the six setters below store native-endian values at
 * offsets those two functions already own, bookkeeping the fake queue can
 * actually see. */
enum { JQATTR_MAXGRAB = 4, JQATTR_FLAGS = 16,
       JQATTR_R0 = 20, JQATTR_R1 = 24 };
enum { JQFLAG_SUBMIT_WITH_ENTRY_LOCK = 1u << 0,
       JQFLAG_DO_BUSY_WAITING        = 1u << 1,
       JQFLAG_IS_HALT_ON_ERROR       = 1u << 2,
       JQFLAG_IS_JOB_TYPE_MEMCHECK   = 1u << 3 };

/* cellVideoOutGetScreenSize errors (SDK cell/cell_video_out.h). */
#define CELL_VIDEO_OUT_ERROR_ILLEGAL_PARAMETER   ((s32)0x8002b222)
#define CELL_VIDEO_OUT_ERROR_UNSUPPORTED_VIDEO_OUT ((s32)0x8002b225)
#define CELL_VIDEO_OUT_ERROR_VALUE_IS_NOT_SET    ((s32)0x8002b228)

static inline u32 ea_of(const void* p) { return (u32)(uintptr_t)p; }

/* ---------------------------------------------------------------------- *
 * SPURS 2.0 instance
 * ---------------------------------------------------------------------- */

/* NID 0x30AA96C4 -- SDK: int cellSpursInitializeWithAttribute2(CellSpurs2*,
 * const CellSpursAttribute*). Same attribute struct as 1.0 (RPCS3 reads it as
 * CellSpursAttribute too); only the instance grows to 8192 bytes. */
s32 sotc_spurs_initialize_with_attribute2(void* spurs, const void* attr)
{
    u32 ea = ea_of(spurs);
    if (!spurs || !attr) return CELL_SPURS_CORE_ERROR_NULL_POINTER;

    /* The real entry point fills CELL_SPURS_SIZE2 bytes (types.h: the extra
     * region exists "so cellSpursInitializeWithAttribute2 can fill a larger
     * region"). The 1.0 path below memsets only 4096; clear the extended
     * half ourselves. The SDK guarantees a CellSpurs2 is 8192 valid,
     * aligned bytes -- same contract the guest is held to. */
    for (u32 o = SPURS_SIZE1; o < SPURS_SIZE2; o += 8) vm_write64(ea + o, 0);

    s32 rc = cellSpursInitializeWithAttribute((CellSpurs*)(uintptr_t)ea,
                                              (const CellSpursAttribute*)attr);
    printf("[sotc] cellSpursInitializeWithAttribute2(ea=0x%08X) -> 0x%08X "
           "(2.0 -> 1.0 shim, instance 8 KB)\n", ea, (u32)rc);
    return rc;
}

/* ---------------------------------------------------------------------- *
 * SPURS 2.0 tasksets
 * ---------------------------------------------------------------------- */

/* NID 0xC2ACDF43 -- SDK: void _cellSpursTasksetAttribute2Initialize(
 * CellSpursTasksetAttribute2*, uint32_t revision). Defaults mirror RPCS3's
 * implementation: name=0, args=0, priority=1 on all SPUs, maxContention=8,
 * enableClearLs=0, taskNameBuffer=0. Returns s32 rather than void so a caller
 * that (unlike the SDK header) looks at r3 sees CELL_OK. */
s32 sotc_taskset_attr2_initialize(void* attr, u32 revision)
{
    u32 ea = ea_of(attr);
    if (!ea) return CELL_SPURS_TASK_ERROR_NULL_POINTER;
    memset(vm_base + ea, 0, TSET2_SIZE);          /* zero is endian-neutral */
    vm_write32(ea + TSET2_REVISION, revision);
    memset(vm_base + ea + TSET2_PRIORITY, 1, 8);  /* byte array */
    vm_write32(ea + TSET2_MAXCONTE, 8);
    return CELL_OK;
}

/* NID 0x4A6465E3 -- SDK: int cellSpursCreateTaskset2(CellSpurs*,
 * CellSpursTaskset2*, const CellSpursTasksetAttribute2*). */
s32 sotc_create_taskset2(void* spurs, void* taskset, const void* attr2)
{
    u32 spurs_ea = ea_of(spurs);
    u32 ts_ea    = ea_of(taskset);
    u32 a2_ea    = ea_of(attr2);
    if (!spurs_ea || !ts_ea) return CELL_SPURS_TASK_ERROR_NULL_POINTER;

    u64 args = a2_ea ? vm_read64(a2_ea + TSET2_ARGS) : 0;
    u8  prio[8];
    if (a2_ea) memcpy(prio, vm_base + a2_ea + TSET2_PRIORITY, 8);
    else       memset(prio, 1, 8);                /* RPCS3's default init */

    /* Synthesize the v1 attribute in the taskset tail (see V1ATTR_* above).
     * Native-endian stores, matching what cellSpurs.c reads back. */
    u32 tmp_ea = ts_ea + V1ATTR_OFF;
    u8* t = vm_base + tmp_ea;
    memset(t, 0, V1ATTR_SIZE);
    *(u32*)(t + V1ATTR_REVISION)    = 2;
    memcpy(t + V1ATTR_ARGS, &args, sizeof args);
    memcpy(t + V1ATTR_PRIORITY, prio, sizeof prio);
    *(u32*)(t + V1ATTR_TASKSETSIZE) = TASKSET2_SIZE;   /* the whole point */

    s32 rc = cellSpursCreateTasksetWithAttribute(
        (CellSpurs*)(uintptr_t)spurs_ea, (CellSpursTaskset*)(uintptr_t)ts_ea,
        (const CellSpursTasksetAttribute*)(uintptr_t)tmp_ea);
    printf("[sotc] cellSpursCreateTaskset2(spurs=0x%08X taskset=0x%08X "
           "size=0x%X) -> 0x%08X (2.0 -> 1.0 shim)\n",
           spurs_ea, ts_ea, TASKSET2_SIZE, (u32)rc);
    return rc;
}

/* NID 0x8ADADF65 -- SDK: void _cellSpursTaskAttribute2Initialize(
 * CellSpursTaskAttribute2*, uint32_t revision). Defaults mirror RPCS3:
 * sizeContext=0, eaContext=0, lsPattern=0, name=0. */
s32 sotc_task_attr2_initialize(void* attr, u32 revision)
{
    u32 ea = ea_of(attr);
    if (!ea) return CELL_SPURS_TASK_ERROR_NULL_POINTER;
    memset(vm_base + ea, 0, TASK2_SIZE);
    vm_write32(ea + TASK2_REVISION, revision);
    return CELL_OK;
}

/* NID 0xE14CA62D -- SDK: int cellSpursCreateTask2(CellSpursTaskset2*,
 * CellSpursTaskId*, const void* eaElf, const CellSpursTaskArgument*,
 * const CellSpursTaskAttribute2*). Five args, so it rides the generic
 * adapter; forward into the 1.0 seven-arg create, which dispatches the task
 * through the runtime's real task machinery (it needs the taskset EA to have
 * been built by a create call -- our CreateTaskset2 above does that). */
s32 sotc_create_task2(void* taskset, void* task_id, const void* elf,
                      const void* argument, const void* attr2)
{
    u32 ts_ea  = ea_of(taskset);
    u32 id_ea  = ea_of(task_id);
    u32 elf_ea = ea_of(elf);
    u32 arg_ea = ea_of(argument);
    u32 a2_ea  = ea_of(attr2);
    if (!ts_ea || !id_ea) return CELL_SPURS_TASK_ERROR_NULL_POINTER;

    u32 size_ctx = 0, ctx_ea = 0, lsp_ea = 0;
    if (a2_ea) {
        size_ctx = vm_read32(a2_ea + TASK2_SIZECONTEXT);
        ctx_ea   = (u32)vm_read64(a2_ea + TASK2_EACONTEXT);
        lsp_ea   = a2_ea + TASK2_LSPATTERN;   /* 16-byte block inside the attr */
    }
    s32 rc = cellSpursCreateTask((CellSpursTaskset*)(uintptr_t)ts_ea,
                                 (CellSpursTaskId*)(uintptr_t)id_ea,
                                 (void*)(uintptr_t)elf_ea,
                                 (void*)(uintptr_t)ctx_ea, size_ctx,
                                 lsp_ea, arg_ea);
    if (rc) fprintf(stderr, "[sotc] cellSpursCreateTask2(taskset=0x%08X "
                    "elf=0x%08X) -> 0x%08X\n", ts_ea, elf_ea, (u32)rc);
    return rc;
}

/* ---------------------------------------------------------------------- *
 * SPURS job queue (Port2 surface)
 * ---------------------------------------------------------------------- */

/* NID 0xF244E799 -- SDK: int _cellSpursCreateJobQueue(jqRevision,
 * sdkRevision, CellSpurs*, CellSpursJobQueue*, const CellSpursJobQueueAttribute*,
 * const char* pName, uint64_t* pCommandList, unsigned depth, unsigned numSpus,
 * const uint8_t priorityTable[8]) -- TEN arguments: r3..r10 carry the first
 * eight, arg9/arg10 sit in the caller's parameter save area. The generic
 * adapter only forwards the eight GPRs, so this one is a ctx handler; stack
 * args at SP+112 onward, the same layout cellSaveData.c and sceNpScore.c read
 * (PPC64 ELFv1: 48-byte fixed frame + parameter save area). */
static void hle_create_jobqueue(ppu_context* ctx)
{
    u32 sp       = (u32)ctx->gpr[1];
    u32 spurs_ea = (u32)ctx->gpr[5];
    u32 jq_ea    = (u32)ctx->gpr[6];
    u32 attr_ea  = (u32)ctx->gpr[7];
    u32 cmd_ea   = (u32)ctx->gpr[9];
    u32 depth    = (u32)ctx->gpr[10];
    u32 num_spus = (u32)vm_read64(sp + 112);       /* arg9  */
    u32 prio_ea  = (u32)vm_read64(sp + 120);       /* arg10 */
    (void)num_spus; (void)prio_ea;

    /* arg1/arg2 (revisions) and arg6 (pName) are advisory; the runtime's
     * queue takes no name and ignores revisions. */
    s32 rc = cellSpursCreateJobQueue((void*)(uintptr_t)spurs_ea,
                                     (CellSpursJobQueue*)(uintptr_t)jq_ea,
                                     (const CellSpursJobQueueAttribute*)(uintptr_t)attr_ea,
                                     (void*)(uintptr_t)cmd_ea, depth);
    printf("[sotc] _cellSpursCreateJobQueue(spurs=0x%08X jq=0x%08X "
           "attr=0x%08X depth=%u) -> 0x%08X (Port2 shim)\n",
           spurs_ea, jq_ea, attr_ea, depth, (u32)rc);
    ctx->gpr[3] = (u64)(s64)rc;
}

/* NID 0xCF89F218 -- SDK: int _cellSpursJobQueuePort2PushJobBody(
 * CellSpursJobQueuePort2*, CellSpursJobHeader*, size_t sizeDesc, unsigned tag,
 * unsigned flag). The runtime's Port2Create aliases the port's first word to
 * the queue slot, so pushing "as the queue" reads that word right back --
 * the same host-pointer trick cellSpursJobQueuePort2PushSync uses upstream.
 * No job id comes back on this entry point (tag/flag are QoS hints the fake
 * queue does not model). */
s32 sotc_jq_port2_push_job_body(void* port2, const void* job, u32 size_desc,
                                u32 tag, u32 flag)
{
    (void)flag;
    return cellSpursJobQueuePush((CellSpursJobQueue*)(uintptr_t)ea_of(port2),
                                 job, size_desc, tag, 0);
}

/* NID 0xFACB3CED -- SDK: int cellSpursJobQueuePort2Sync(
 * CellSpursJobQueuePort2*, unsigned flag). Wait for outstanding pushes: the
 * Jq HLE marks a job complete inside the push itself, so there is never
 * anything outstanding. */
s32 sotc_jq_port2_sync(void* port2, u32 flag)
{
    (void)port2; (void)flag;
    return CELL_OK;
}

/* ---- the six attribute setters (SDK job_queue.h prototypes) ------------
 * bool/int values arrive in r4 (and r5): a PPC bool is a zero-extended byte. */

static inline u8* jq_attr(void* attr)   /* host view of the opaque bag */
{
    return vm_base + ea_of(attr);
}

/* NID 0x0F03F712 */ /* SetSubmitWithEntryLock(attr, bool) */
s32 sotc_jq_set_submit_with_entry_lock(void* attr, u32 on)
{
    if (!attr) return CELL_SPURS_JQ_ERROR_INVALID_ARGUMENT;
    u8* p = jq_attr(attr);
    p[JQATTR_FLAGS] = on ? (u8)(p[JQATTR_FLAGS] | JQFLAG_SUBMIT_WITH_ENTRY_LOCK)
                         : (u8)(p[JQATTR_FLAGS] & ~JQFLAG_SUBMIT_WITH_ENTRY_LOCK);
    return CELL_OK;
}

/* NID 0x0582338A */ /* SetDoBusyWaiting(attr, bool) */
s32 sotc_jq_set_do_busy_waiting(void* attr, u32 on)
{
    if (!attr) return CELL_SPURS_JQ_ERROR_INVALID_ARGUMENT;
    u8* p = jq_attr(attr);
    p[JQATTR_FLAGS] = on ? (u8)(p[JQATTR_FLAGS] | JQFLAG_DO_BUSY_WAITING)
                         : (u8)(p[JQATTR_FLAGS] & ~JQFLAG_DO_BUSY_WAITING);
    return CELL_OK;
}

/* NID 0xFF03CC79 */ /* SetIsHaltOnError(attr, bool) */
s32 sotc_jq_set_is_halt_on_error(void* attr, u32 on)
{
    if (!attr) return CELL_SPURS_JQ_ERROR_INVALID_ARGUMENT;
    u8* p = jq_attr(attr);
    p[JQATTR_FLAGS] = on ? (u8)(p[JQATTR_FLAGS] | JQFLAG_IS_HALT_ON_ERROR)
                         : (u8)(p[JQATTR_FLAGS] & ~JQFLAG_IS_HALT_ON_ERROR);
    return CELL_OK;
}

/* NID 0x742CEC0D */ /* SetIsJobTypeMemoryCheck(attr, bool) */
s32 sotc_jq_set_is_job_type_memory_check(void* attr, u32 on)
{
    if (!attr) return CELL_SPURS_JQ_ERROR_INVALID_ARGUMENT;
    u8* p = jq_attr(attr);
    p[JQATTR_FLAGS] = on ? (u8)(p[JQATTR_FLAGS] | JQFLAG_IS_JOB_TYPE_MEMCHECK)
                         : (u8)(p[JQATTR_FLAGS] & ~JQFLAG_IS_JOB_TYPE_MEMCHECK);
    return CELL_OK;
}

/* NID 0x1686957E */ /* SetMaxSizeJobDescriptor(attr, unsigned) */
s32 sotc_jq_set_max_size_job_descriptor(void* attr, u32 size)
{
    if (!attr) return CELL_SPURS_JQ_ERROR_INVALID_ARGUMENT;
    /* Reserved slot, host-endian: this runtime's queue never sizes
     * descriptors (every push completes inline), but keep the value where a
     * future real implementation can find it. */
    memcpy(jq_attr(attr) + JQATTR_R1, &size, sizeof size);
    return CELL_OK;
}

/* NID 0xE70F874E */ /* SetGrabParameters(attr, unsigned maxNumJobsOnSpu,
                                 unsigned maxGrab) */
s32 sotc_jq_set_grab_parameters(void* attr, u32 max_jobs_on_spu, u32 max_grab)
{
    if (!attr) return CELL_SPURS_JQ_ERROR_INVALID_ARGUMENT;
    u8* p = jq_attr(attr);
    memcpy(p + JQATTR_R0, &max_jobs_on_spu, sizeof max_jobs_on_spu);
    memcpy(p + JQATTR_MAXGRAB, &max_grab, sizeof max_grab);  /* live field */
    return CELL_OK;
}

/* ---------------------------------------------------------------------- *
 * cellSysutilAvconfExt
 * ---------------------------------------------------------------------- */

/* NID 0xFAA275A4 -- the table's "cellSysutilAvconfExt (unnamed)" entry. The
 * public NID databases (clienthax/Ps3GhidraScripts, zecoxao/sce_symbols,
 * FirebirdTA01/PS3DK's libsysutil_avconf_ext_stub extraction) all name it
 * cellVideoOutGetScreenSize, and the SDK header carries it in
 * cell_video_out.h under the avconf_ext stub set -- SDK:
 * int cellVideoOutGetScreenSize(uint32_t videoOut, float* screenSize).
 * Behaviour mirrors RPCS3's cellAvconfExt.cpp, which is proven against this
 * exact title: no stereo/3D display is configured in this runtime, so the
 * value is not set and the boot continues. */
s32 sotc_video_out_get_screen_size(u32 video_out, void* screen_size)
{
    if (!screen_size) return CELL_VIDEO_OUT_ERROR_ILLEGAL_PARAMETER;
    if (video_out != 0) return CELL_VIDEO_OUT_ERROR_UNSUPPORTED_VIDEO_OUT;
    return CELL_VIDEO_OUT_ERROR_VALUE_IS_NOT_SET;
}

/* ---------------------------------------------------------------------- */

__attribute__((constructor)) static void sotc_hle_overrides(void)
{
    /* 14 generic-ABI handlers (<= 8 args, raw GPRs in, s32 out) + 1 ctx
     * handler for the ten-argument _cellSpursCreateJobQueue. NIDs are literal:
     * each one was verified against ps3recomp's nid_database.compute_nid. */
    static const struct { uint32_t nid; const char* name; void* fn; } gen[] = {
        { 0x30AA96C4u, "cellSpursInitializeWithAttribute2",
          (void*)sotc_spurs_initialize_with_attribute2 },
        { 0x4A6465E3u, "cellSpursCreateTaskset2",
          (void*)sotc_create_taskset2 },
        { 0xC2ACDF43u, "_cellSpursTasksetAttribute2Initialize",
          (void*)sotc_taskset_attr2_initialize },
        { 0x8ADADF65u, "_cellSpursTaskAttribute2Initialize",
          (void*)sotc_task_attr2_initialize },
        { 0xE14CA62Du, "cellSpursCreateTask2",
          (void*)sotc_create_task2 },
        { 0xCF89F218u, "_cellSpursJobQueuePort2PushJobBody",
          (void*)sotc_jq_port2_push_job_body },
        { 0xFACB3CEDu, "cellSpursJobQueuePort2Sync",
          (void*)sotc_jq_port2_sync },
        { 0x0F03F712u, "cellSpursJobQueueAttributeSetSubmitWithEntryLock",
          (void*)sotc_jq_set_submit_with_entry_lock },
        { 0x0582338Au, "cellSpursJobQueueAttributeSetDoBusyWaiting",
          (void*)sotc_jq_set_do_busy_waiting },
        { 0xFF03CC79u, "cellSpursJobQueueAttributeSetIsHaltOnError",
          (void*)sotc_jq_set_is_halt_on_error },
        { 0x742CEC0Du, "cellSpursJobQueueAttributeSetIsJobTypeMemoryCheck",
          (void*)sotc_jq_set_is_job_type_memory_check },
        { 0x1686957Eu, "cellSpursJobQueueAttributeSetMaxSizeJobDescriptor",
          (void*)sotc_jq_set_max_size_job_descriptor },
        { 0xE70F874Eu, "cellSpursJobQueueAttributeSetGrabParameters",
          (void*)sotc_jq_set_grab_parameters },
        { 0xFAA275A4u, "cellVideoOutGetScreenSize",
          (void*)sotc_video_out_get_screen_size },
    };
    for (unsigned i = 0; i < sizeof gen / sizeof gen[0]; i++)
        ps3_hle_register(gen[i].nid, gen[i].name, gen[i].fn);
    ps3_hle_register_ctx(0xF244E799u, "_cellSpursCreateJobQueue",
                         hle_create_jobqueue);
    fprintf(stderr, "[sotc] registered 15 HLE shims "
            "(SPURS 2.0 tasksets, Jq Port2, avconf)\n");
}
