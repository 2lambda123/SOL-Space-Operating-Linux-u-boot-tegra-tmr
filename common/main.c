/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* #define	DEBUG	*/

#include <common.h>
#include <autoboot.h>
#include <cli.h>
#include <console.h>
#include <malloc.h>
#include <version.h>
#include <../common/tmr.c>
#include <command.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Board-specific Platform code can reimplement show_boot_progress () if needed
 */
__weak void show_boot_progress(int val) {}

static void run_preboot_environment_command(void)
{
#ifdef CONFIG_PREBOOT
	char *p;

	p = getenv("preboot");
	if (p != NULL) {
# ifdef CONFIG_AUTOBOOT_KEYED
		int prev = disable_ctrlc(1);	/* disable Control C checking */
# endif

		run_command_list(p, -1, 0);

# ifdef CONFIG_AUTOBOOT_KEYED
		disable_ctrlc(prev);	/* restore Control C checking */
# endif
	}
#endif /* CONFIG_PREBOOT */
}

/* We come here after U-Boot is initialised and ready to process commands */
void main_loop(void)
{
	const char *s;

	bootstage_mark_name(BOOTSTAGE_ID_MAIN_LOOP, "main_loop");

#ifdef CONFIG_VERSION_VARIABLE
	setenv("ver", version_string);  /* set version variable */
#endif /* CONFIG_VERSION_VARIABLE */

	cli_init();

	run_preboot_environment_command();

#if defined(CONFIG_UPDATE_TFTP)
	update_tftp(0UL, NULL, NULL);
#endif /* CONFIG_UPDATE_TFTP */

	s = bootdelay_process();
	if (cli_process_fdt(&s))
		cli_secure_boot_cmd(s);
    printf("meta-sol ref YOCTO_SOL_REF\n");
    fs_set_blk_dev("mmc", "0:1", FS_TYPE_EXT);

    // Collecting file offsets:
    // There are four files and four hashes per partition (no rootfs at this stage), and each is placed one after the other.
    // Info file starts at zero offset (mem addr 0xa5000000) and is 1 block long (512 bytes). Hashes are also one block. The remaining files (excluding rootfs) have their 
    // offsets and sizes defined in the "BLOB Stuff" section of the machine configuration in Yocto
    ulong file_offset[8] = {YOCTO_INFO_FILE_OFFSET, YOCTO_INFO_HASH_OFFSET,
                            YOCTO_IMAGE_FILE_OFFSET, YOCTO_IMAGE_HASH_OFFSET,
                            YOCTO_DTB_FILE_OFFSET, YOCTO_DTB_HASH_OFFSET,
                            YOCTO_INITRD_FILE_OFFSET, YOCTO_INITRD_HASH_OFFSET};
    ulong part_size = YOCTO_ROOTFSPART_SIZE / YOCTO_BLOCK_SIZE;
    ulong part_offset[3] = {YOCTO_PARTITION_OFFSET,
                            YOCTO_PARTITION_OFFSET + part_size,
                            YOCTO_PARTITION_OFFSET + (2 * part_size)};
    ulong outputs[4];
    outputs[0] = 0xa5000000; // in u-boot memory
    outputs[1] = outputs[0] + YOCTO_INFO_FILE_BLOCKS * YOCTO_BLOCK_SIZE;
    outputs[2] = outputs[1] + YOCTO_IMAGE_FILE_BLOCKS * YOCTO_BLOCK_SIZE;
    outputs[3] = outputs[2] + YOCTO_DTB_FILE_BLOCKS * YOCTO_BLOCK_SIZE;
    ulong sizes[4];
    sizes[0] = YOCTO_INFO_BYTES * 4; //info file, 4 filesizes
    char *safe = "s=----";

if (YOCTO_SOL_ENABLE_TMR) {
     
    printf("TMRing info files\n");
    if(
        tmr_blob(
            part_offset[0] + file_offset[0],
            part_offset[0] + file_offset[1],
            part_offset[1] + file_offset[0],
            part_offset[1] + file_offset[1],
            part_offset[2] + file_offset[0],
            part_offset[2] + file_offset[1],
            sizes[0], outputs[0])
    ) {
        safe[2] = 'x';
    }

    printf("Reading info for filesizes\n");
    char tmp[YOCTO_INFO_BYTES];
    char *info = (char *)map_sysmem(outputs[0], YOCTO_INFO_BYTES*3);
    for (int i = 0; i < 3; i ++) {
        memcpy(tmp, info + YOCTO_INFO_BYTES * i, YOCTO_INFO_BYTES);
        sizes[i+1] = simple_strtoul(tmp, NULL, 10);
    }
    unmap_sysmem((const void*) info);

    for (int i = 1; i < 4; i ++) {
        printf("TMRing file #%d of size %lu\n", i, sizes[i]);
        if(
            // Replace this with 
            // outputs[i] 
            // part_offset[0] first partition
            // file_offset[xxx] start of first file
            
            !tmr_blob(
                part_offset[0] + file_offset[2*i],
                part_offset[0] + file_offset[2*i+1],
                part_offset[1] + file_offset[2*i],
                part_offset[1] + file_offset[2*i+1],
                part_offset[2] + file_offset[2*i],
                part_offset[2] + file_offset[2*i+1],
                sizes[i], outputs[i])
        ) {
            // pass info to kernel
            safe[i+2] = 'x';
        }
    }
} else {
    printf("TMR DISABLED - No Hash Checks\n");
    printf("Copying files directly to memory.\n");
    memcpy((void *)outputs[0], (void *)part_offset[0] + file_offset[0], sizes[0]);  // Info file 
    memcpy((void *)outputs[1], (void *)part_offset[0] + file_offset[2], sizes[1]);  // Image File
    memcpy((void *)outputs[2], (void *)part_offset[0] + file_offset[4], sizes[2]);  // DTB File
    memcpy((void *)outputs[3], (void *)part_offset[0] + file_offset[6], sizes[3]);  // Initrd File
}
    // TODO: See if we can find a way to make use of bootargs
    //setenv("bootargs", safe);
    char bootargs[CONFIG_SYS_CBSIZE];
    cli_simple_process_macros("${cbootargs} root=/dev/ram0 rw rootwait ${bootargs}", bootargs);
    setenv("bootargs", bootargs);

    if(!abortboot(5)) {
        char initrd_loc[YOCTO_INFO_BYTES*2] = "";
        sprintf(initrd_loc, "%x:%x", outputs[3], sizes[3]);
		char image_loc[YOCTO_INFO_BYTES] = "";
        sprintf(image_loc, "%x", outputs[1]);
        char dtb_loc[YOCTO_INFO_BYTES] = "";
        sprintf(dtb_loc, "%x", outputs[2]);
        char *argv[4] = {"booti", image_loc,
                         initrd_loc,
                         dtb_loc};
        cmd_tbl_t *bcmd = find_cmd("booti");
        do_booti(bcmd, 0, 4, argv);
    } else {
        cli_loop();
        panic("No CLI available");
    }

}
