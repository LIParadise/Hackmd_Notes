# Transfer OS on GPT disk to another smaller disk

So I got myself a Orico RTL9210-based nvme to USB enclosure. Didn't bothered to do a detailed test on the performance, though, since on my laptop with Crystal Disk Mark, two of my enclosures perform nearly the same on 5Gbps USB port, one being Samsung 860 Evo 1TB in Transcend 25s3 (ASM1153E), one being Samsung pm981 512GB in Orico RTL9210-based enclosure, so I figured it's the 5Gbps USB controller of my laptop being the bottleneck and can't really measure anything useful based on it. Anyway I tried to migrate my Windows installation on the sata 1TB disk to the nvme 512GB disk, for that the latter is significantly smaller and lighter, that I don't really need 1TB Windows laptop, that with RTL9210 I can use 10Gbps USB ports, that the 1TB disk can now serve as dedicated disk image storage for VM, and finally that maybe random performance is indeed better on pm981 on RTL9210, it's just the difference wasn't recorded by CDM.



## Get Ready to `rm -rf --no-preserve-root /`

Jokes aside, be sure to back up any important data.

You would need a (presumably live CD) Linux environment, with `fdisk`, `sfdisk`, `gdisk`, and optionally `gparted`. In my case `gdisk` is provided by `gptfdisk` package, `sfdisk` by `util-linux`. BTW I use Arch.

In the following section, the old disk to be copied from is `/dev/sdo`, `o` for old, and the new disk to be copied to is `/dev/sdn`, `n` for new. Assume the old disk is using GPT partition.

1. Shrink the partitions on the old drive such that they fit in the new drive

   `gparted` is for now if you don't want any math.

2. Backup partition tables

   ``` bash
   sfdisk -d /dev/sdo > old_gpt
   ```

3. Copy the partition table to the new drive (<cite>[tecmint][1]</cite>)

   ``` bash
   dd if=/dev/sdo of=/dev/sdn bs=2048 count=1 conv=noerror,sync
   ```

   Notice that

   > GPT partition style you should clone the first 2048 bytes:
   >
   > -- <cite>[tecmint][1]</cite>

   Since we've shrunk the partitions, the table should work, except it's not since it records the old, now wrong size of disk, and that it disagrees with the backup GPT table on the new disk. Basically we have a corrupted GPT table on the new drive now.

   `conv` part of the options is to

   > specify that every byte is copied to the same physical location (from the start).
   >
   > -- <cite>[superuser][2]</cite>

4. Fix the GPT table on the new drive

   ``` bash
   gdisk /dev/sdn
   ```

   Then, `r` for `recovery and transformation options (experts only)`, `d` for `use main GPT header (rebuilding backup)`, then `w` for `write table to disk and exit`. `gdisk` should also fix the problem that GPT and disk physically don't agree on size when writing table.

   We use the main GPT header since that's exactly what we just `dd` from old drive.

   After this, the corrupted GPT should now be valid, with partition scheme the same as that of the old drive.

5. Start copying things. (<cite>[superuser][2]</cite>)

   ``` bash
   dd if=/dev/sdo1 of=/dev/sdn1 conv=noerror,sync bs=64K
   ```

   Remember to do all the required partitions.

   If everything goes well, now the two disks should be practically the same disk: same partition table, same contents, same partition labels, UUID, and PARTUUID even. Except of course physical sizes are different.

6. Aftermath

   Generally one don't want UUID/PARTUUID to collide, so be sure to change that on at least one of the drives out, or just don't use them on same computer.

   One way to change UUID is to use `sfdisk` again, but this time remember also to

   > `sfdisk` will generate new UUIDs if you delete the references to partitions ids (`, uuid=...`) and the reference to the disk id (`label-id: ...`) from the dump.
   >
   > -- <cite>[unix stackexchange][3]</cite>

   After proper modification is done, let's say I want to give the old disk new (PART)UUID, import the partition table to the corresponding disk.

   ``` bash
   sfdisk /dev/sdo < old_gpt
   ```

   Or just use other methods you're familiar with, maybe `gparted` again.

   And... voilà! You now have an exact copy of the old drive, except it's now on a new drive!



## Final Remarks

- For MBR drives, things need to be modified. At least the `bs` in `dd` should be `512` (<cite>[techmint][1]</cite>). I'm not so sure about if MBR needs to or can be fixed like above with `gdisk` though.
- So after turning my now-migrated Windows on, I found something weird: drive optimization don't show `C:\`, i.e. my system disk aka pm981 in Orico RTL9210-based USB enclosure. Any trim method also don't work, including `defrag` and `optimize-volume` in Windows and `fstrim` in Linux. This used not to be the case. So I ran `sfc /cannow` and `DISM /Online /Cleanup-Image /RestoreHealth`, but it didn't help. Don't have the slightest clue, I put the USB enclosure onto my PC. Turns out there seems to be something wrong with the NTFS: my PC Windows warned me the drive may have some problems as soon as I plugged in, and a `chkdsk` run stopped with an `unexpected error`. Out of despair I tried re-plug in PC once again, but this time use the GUI dialog to "scan and fix". It (miraculously) worked: now the drive can be trimmed normally in Windows, and none of further runs of `chkdsk`, `sfc`, or `dism` reveal any problems.



## References

<cite>[tecmint][1]</cite>
<cite>[superuser][2]</cite>
<cite>[unix stackexchange][3]</cite>

[1]: https://www.tecmint.com/migrate-windows-10-from-hdd-to-ssd-using-clonezilla/
[2]: https://superuser.com/a/1050914/936873
[3]: https://unix.stackexchange.com/a/12988/312080