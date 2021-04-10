/* 
 ****************************************************************************
 * (C) 2006 - Cambridge University
 ****************************************************************************
 *
 *        File: gnttab.c
 *      Author: Steven Smith (sos22@cam.ac.uk) 
 *     Changes: Grzegorz Milos (gm281@cam.ac.uk)
 *              
 *        Date: July 2006
 * 
 * Environment: Xen Minimal OS
 * Description: Simple grant tables implementation. About as stupid as it's
 *  possible to be and still work.
 *
 ****************************************************************************
 */
#include <types.h>
#include <gnttab.h>
#include <memory.h>
#include <printf.h>
#include <os.h>

#define GNTTAB_PAGE_SIZE 4096U
#define GNTTAB_PAGE_SHIFT 12U

#define NR_RESERVED_ENTRIES 8

/* NR_GRANT_FRAMES must be less than or equal to that configured in Xen */
#define NR_GRANT_FRAMES 1
#define NR_GRANT_ENTRIES (NR_GRANT_FRAMES * GNTTAB_PAGE_SIZE / sizeof(grant_entry_v1_t))

grant_entry_v1_t *gnttab_table;

static grant_ref_t gnttab_list[NR_GRANT_ENTRIES];

static void
put_free_entry(grant_ref_t ref)
{
    gnttab_list[ref] = gnttab_list[0];
    gnttab_list[0]  = ref;
}

static grant_ref_t
get_free_entry(void)
{
    unsigned int ref;
    ref = gnttab_list[0];
    gnttab_list[0] = gnttab_list[ref];
    return ref;
}

grant_ref_t
gnttab_grant_access(domid_t domid, unsigned long frame, int readonly)
{
    grant_ref_t ref;

    ref = get_free_entry();
    gnttab_table[ref].frame = frame;
    gnttab_table[ref].domid = domid;
    wmb();
    readonly *= GTF_readonly;
    gnttab_table[ref].flags = GTF_permit_access | readonly;

    return ref;
}

grant_ref_t
gnttab_grant_transfer(domid_t domid, unsigned long pfn)
{
    grant_ref_t ref;

    ref = get_free_entry();
    gnttab_table[ref].frame = pfn;
    gnttab_table[ref].domid = domid;
    wmb();
    gnttab_table[ref].flags = GTF_accept_transfer;

    return ref;
}

int
gnttab_end_access(grant_ref_t ref)
{
    uint16_t flags, nflags;

    nflags = gnttab_table[ref].flags;
    do {
        if ((flags = nflags) & (GTF_reading|GTF_writing)) {
            printf("WARNING: g.e. still in use! (%x)\n", flags);
            return 0;
        }
    } while ((nflags = synch_cmpxchg(&gnttab_table[ref].flags, flags, 0)) !=
            flags);

    put_free_entry(ref);
    return 1;
}

unsigned long
gnttab_end_transfer(grant_ref_t ref)
{
    unsigned long frame;
    uint16_t flags;

    while (!((flags = gnttab_table[ref].flags) & GTF_transfer_committed)) {
        if (synch_cmpxchg(&gnttab_table[ref].flags, flags, 0) == flags) {
            printf("Release unused transfer grant.\n");
            put_free_entry(ref);
            return 0;
        }
    }

    /* If a transfer is in progress then wait until it is completed. */
    while (!(flags & GTF_transfer_completed)) {
        flags = gnttab_table[ref].flags;
    }

    /* Read the frame number /after/ reading completion status. */
    rmb();
    frame = gnttab_table[ref].frame;

    put_free_entry(ref);

    return frame;
}

void
init_gnttab(void)
{
    struct xen_add_to_physmap xatp;
    unsigned long page = (unsigned long) gnttab_table >> GNTTAB_PAGE_SHIFT;
    int i;

    for (i = NR_RESERVED_ENTRIES; i < NR_GRANT_ENTRIES; i++)
        put_free_entry(i);

    i = NR_GRANT_FRAMES;
    do {
        i--;
        xatp.domid = DOMID_SELF;
        xatp.idx = i;
        xatp.space = XENMAPSPACE_grant_table;
        xatp.gpfn = page + i;
        if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp)) {
            printf("cannot map gnttab_table!");
			while (1) {} // halt the system
		}
    } while (i != 0);

    printf("gnttab_table mapped at %p.\n", gnttab_table);
}

void
fini_gnttab(void)
{
    struct gnttab_setup_table setup;

    setup.dom = DOMID_SELF;
    setup.nr_frames = 0;

    HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup, 1);
}
