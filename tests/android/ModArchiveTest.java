package org.re1.launcher;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public final class ModArchiveTest {
    interface Attempt {void run() throws Exception;}
    static void reject(Attempt action) throws Exception {
        try{action.run();throw new AssertionError("Expected invalid mod rejection");}catch(IOException expected){}
    }
    static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    static void rejectMessage(Attempt action,String text) throws Exception {
        try{action.run();throw new AssertionError("Expected rejection: "+text);}catch(IOException error){check(error.getMessage().contains(text),error.getMessage());}
    }
    static byte[] zip(String[] names,byte[][] data) throws IOException {
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();
        try(ZipOutputStream zip=new ZipOutputStream(bytes)){
            for(int n=0;n<names.length;n++){zip.putNextEntry(new ZipEntry(names[n]));zip.write(data[n]);zip.closeEntry();}
        }
        return bytes.toByteArray();
    }
    static final ModArchive.Progress progress=(done,total,phase)->{};
    public static void main(String[] args) throws Exception {
        File temp=Files.createTempDirectory("re1-mod-archive-").toFile();
        ModArchive store=new ModArchive(new File(temp,"mods"),new File(temp,"cache"));
        byte[] binary=new byte[600000];for(int n=0;n<binary.length;n++)binary[n]=(byte)(n*37+11);
        byte[] archive=zip(new String[]{"Example/mod.json","Example/files/Enemy/EM1111.EMD"},new byte[][]{"{}".getBytes(),binary});
        String kept;
        try(ModArchive.Import opened=store.open(new ByteArrayInputStream(archive),archive.length,()->false,progress)){
            check(opened.items.size()==2,"ZIP entries missing");kept=opened.id;
            check(Arrays.equals(opened.read(1,131072,262144),Arrays.copyOfRange(binary,131072,393216)),"Range transport changed bytes");
            reject(()->opened.read(1,-1,1));reject(()->opened.read(1,0,262145));reject(()->opened.read(1,binary.length,1));
            opened.commit(new int[]{1},progress);
            check(Arrays.equals(store.read(kept,1,0,262144),Arrays.copyOfRange(binary,0,262144)),"Committed bytes changed");
            check(!new File(temp,"mods/"+kept+"/0.bin").exists(),"Unselected texture/metadata was extracted");
        }
        check(new File(temp,"cache").list().length==0,"Import ZIP was retained after close");
        for(String path:new String[]{"../outside","/absolute","C:/outside","a/../../b","a//b","a/./b","a\u0000b"})reject(()->ModArchive.path(path));
        reject(()->store.open(new ByteArrayInputStream(zip(new String[]{"../escape"},new byte[][]{binary})), -1,()->false,progress));
        reject(()->store.open(new ByteArrayInputStream(zip(new String[]{"A.TIM","a.tim"},new byte[][]{{1},{2}})), -1,()->false,progress));
        reject(()->store.open(new ByteArrayInputStream(zip(new String[]{"files/a","files/a/b"},new byte[][]{{1},{2}})), -1,()->false,progress));
        reject(()->store.open(new ByteArrayInputStream(archive),archive.length+1,()->false,progress));
        reject(()->store.open(new ByteArrayInputStream(new byte[]{1,2,3}),-1,()->false,progress));
        for(byte[] header:new byte[][]{{0x52,0x61,0x72,0x21,0x1a,7,0},{0x52,0x61,0x72,0x21,0x1a,7,1,0}}){
            byte[] rar=Arrays.copyOf(header,64);
            rejectMessage(()->store.open(new ByteArrayInputStream(rar),rar.length,()->false,progress),"This is a RAR archive");
        }
        byte[] seven=Arrays.copyOf(new byte[]{0x37,0x7a,(byte)0xbc,(byte)0xaf,0x27,0x1c},64);
        rejectMessage(()->store.open(new ByteArrayInputStream(seven),seven.length,()->false,progress),"This is a 7z archive");
        byte[] truncated=Arrays.copyOf(archive,archive.length-30);
        rejectMessage(()->store.open(new ByteArrayInputStream(truncated),truncated.length,()->false,progress),"Could not read this ZIP");
        try(ModArchive.Import opened=store.open(new ByteArrayInputStream(archive){
            boolean zero=true;
            @Override public synchronized int read(byte[] b,int at,int length){if(zero){zero=false;return 0;}return super.read(b,at,length);}
        },archive.length,()->false,progress)){
            check(opened.items.size()==2,"A zero-byte stream read truncated the ZIP");
        }
        AtomicBoolean cancel=new AtomicBoolean();
        reject(()->store.open(new ByteArrayInputStream(archive),archive.length,cancel::get,(done,total,phase)->cancel.set(true)));
        try(ModArchive.Import failed=store.open(new ByteArrayInputStream(archive),-1,()->false,progress)){
            reject(()->failed.commit(new int[]{1,1},progress));
            check(!new File(temp,"mods/"+failed.id).exists(),"Failed installation left a committed mod");
            check(new File(temp,"mods/"+kept).isDirectory(),"Failed import changed an existing mod");
        }
        reject(()->store.read("../outside",0,0,1));reject(()->store.remove("../outside"));
        store.remove(kept);check(!new File(temp,"mods/"+kept).exists(),"Remove did not delete imported mod");
        check(Arrays.equals(ModArchive.mediaRange("bytes=100-",600000),new long[]{100,599900}),"Open media range");
        check(Arrays.equals(ModArchive.mediaRange("bytes=-32",600000),new long[]{599968,32}),"Suffix media range");
        for(String range:new String[]{"bytes=600000-","bytes=40-20","bytes=-0","bytes=0-3,9-20","bytes=999999999999999999999999-"})reject(()->ModArchive.mediaRange(range,600000));
        File movie=new File(temp,"sample.mp4");Files.write(movie.toPath(),binary);
        try(java.io.InputStream stream=ModArchive.mediaStream(movie,100,16)){
            check(Arrays.equals(stream.readAllBytes(),Arrays.copyOfRange(binary,100,116)),"Media stream escaped its range");
            check(stream.read()==-1,"Media stream did not end");
        }
        check(new File(temp,"cache").list().length==0,"Rejected/cancelled ZIP leaked staging files");
        System.out.println("PASS: native ZIP binary reads, selected extraction, duplicate/traversal/range rejection, cancellation, rollback and private-only removal");
        // Optional real-pack validation. Assets remain at the supplied private
        // path; only the transport's own temporary ZIP copy is made and removed.
        if(args.length==1){
            File real=new File(args[0]);
            try(FileInputStream source=new FileInputStream(real);ModArchive.Import opened=store.open(source,real.length(),()->false,progress)){
                int checked=0;
                for(ModArchive.Item item:opened.items)if(item.path.endsWith(".webp")&&item.size<1048576){
                    java.util.zip.CRC32 crc=new java.util.zip.CRC32();
                    for(long at=0;at<item.size;at+=ModArchive.CHUNK){byte[] bytes=opened.read(item.slot,at,(int)Math.min(ModArchive.CHUNK,item.size-at));crc.update(bytes);}
                    check(crc.getValue()==item.crc,"Real-pack chunk CRC mismatch: "+item.path);
                    if(++checked==8)break;
                }
                check(checked>0,"No real-pack textures were tested");
                for(ModArchive.Item item:opened.items)if(item.path.equalsIgnoreCase("movie/Capcom.mp4")){
                    opened.commit(new int[]{item.slot},progress);
                    File media=store.mediaFile(opened.id,item.slot);check(media.length()==item.size,"Installed MP4 size changed");
                    long offset=item.size-100000;byte[] expected=opened.read(item.slot,offset,100000);
                    try(java.io.InputStream stream=ModArchive.mediaStream(media,offset,100000)){
                        check(Arrays.equals(stream.readAllBytes(),expected),"Installed MP4 tail/range bytes changed");
                    }
                    store.remove(opened.id);System.out.println("PASS: real MP4 extraction CRC and bounded media tail streaming");break;
                }
                System.out.println("PASS: native full ZIP validation ("+opened.items.size()+" files), sampled WebP chunk CRCs");
            }
            check(new File(temp,"cache").list().length==0,"Real ZIP staging copy was retained");
        }
    }
}
