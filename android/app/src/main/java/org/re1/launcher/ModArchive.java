package org.re1.launcher;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;
import java.util.function.BooleanSupplier;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipException;

/** Port-owned ZIP transport. Imported bytes never enter the original game folder.
 * Texture conversion is performed by the launcher worker. Only selected data
 * overrides are copied out of the ZIP; no imported program is ever executed.
 */
final class ModArchive {
    static final long MAX_ZIP=4L*1024*1024*1024, MAX_TOTAL=8L*1024*1024*1024;
    static final long MAX_FILE=1024L*1024*1024;
    static final int MAX_FILES=50000, CHUNK=262144;
    interface Progress {void update(long done,long total,String phase) throws IOException;}
    static final class Item {
        final int slot;final String path;final long size,crc;
        Item(int slot,String path,long size,long crc){this.slot=slot;this.path=path;this.size=size;this.crc=crc;}
    }
    static String path(String input) throws IOException {
        if(input==null||input.length()>1024)throw new IOException("Invalid ZIP path");
        String name=input.replace('\\','/');
        if(name.endsWith("/"))name=name.substring(0,name.length()-1);
        String[] parts=name.split("/",-1);
        if(parts.length>20)throw new IOException("ZIP path is too deeply nested");
        for(String part:parts){
            if(part.isEmpty()||part.equals(".")||part.equals("..")||part.length()>255)
                throw new IOException("Unsafe ZIP path: "+input);
            for(int n=0;n<part.length();n++)if(part.charAt(n)<32||part.charAt(n)==127||part.charAt(n)==':')
                throw new IOException("Unsafe ZIP path: "+input);
        }
        return name;
    }
    static boolean id(String value){return value!=null&&value.matches("pack-[a-f0-9]{32}");}
    private final File root,cache;
    ModArchive(File root,File cache) throws IOException {
        this.root=root;this.cache=cache;
        if((!root.isDirectory()&&!root.mkdirs())||(!cache.isDirectory()&&!cache.mkdirs()))throw new IOException("Cannot open mod storage");
    }
    private static void check(BooleanSupplier cancelled) throws IOException {
        if(cancelled.getAsBoolean()||Thread.currentThread().isInterrupted())throw new IOException("Import cancelled");
    }
    private static void checkFormat(File file) throws IOException {
        byte[] signature=new byte[8];int size;
        try(RandomAccessFile input=new RandomAccessFile(file,"r")){size=input.read(signature);}
        if(size>=7&&signature[0]==0x52&&signature[1]==0x61&&signature[2]==0x72&&signature[3]==0x21&&signature[4]==0x1a&&signature[5]==7)
            throw new IOException("This is a RAR archive, not a ZIP. Extract it first, then ZIP the extracted game folders (for example Enemy). Renaming .rar to .zip does not convert it.");
        if(size>=6&&signature[0]==0x37&&signature[1]==0x7a&&(signature[2]&255)==0xbc&&(signature[3]&255)==0xaf&&signature[4]==0x27&&signature[5]==0x1c)
            throw new IOException("This is a 7z archive. Extract it first, then create a ZIP containing the extracted game folders.");
    }
    final class Import implements AutoCloseable {
        final String id="pack-"+UUID.randomUUID().toString().replace("-","");
        final File archive=new File(cache,id+".zip");
        final List<Item> items=new ArrayList<>();
        final List<ZipEntry> entries=new ArrayList<>();
        final BooleanSupplier cancelled;
        ZipFile zip;boolean committed;
        Import(BooleanSupplier cancelled){this.cancelled=cancelled;}
        byte[] read(int slot,long offset,int length) throws IOException {
            check(cancelled);if(slot<0||slot>=items.size())throw new IOException("Unknown mod entry");
            Item item=items.get(slot);FileChunks.validate(item.size,offset,length);
            try(InputStream stream=zip.getInputStream(entries.get(slot))){return FileChunks.read(stream,item.size,offset,length);}
        }
        void commit(int[] slots,Progress progress) throws IOException {
            check(cancelled);
            File stage=new File(root,"stage-"+id),dest=new File(root,id);
            if(stage.exists()||dest.exists()||!stage.mkdir())throw new IOException("Cannot stage mod data");
            boolean done=false;long total=0,written=0;Set<Integer> unique=new HashSet<>();
            try{
                for(int slot:slots){
                    if(slot<0||slot>=items.size()||!unique.add(slot))throw new IOException("Invalid mod data entry");
                    total+=items.get(slot).size;
                }
                byte[] buffer=new byte[131072];
                for(int slot:slots){
                    Item item=items.get(slot);CRC32 crc=new CRC32();long count=0;
                    try(InputStream input=zip.getInputStream(entries.get(slot));FileOutputStream output=new FileOutputStream(new File(stage,slot+".bin"))){
                        for(int n;(n=input.read(buffer))!=-1;){
                            check(cancelled);if(n==0){int one=input.read();if(one<0)break;buffer[0]=(byte)one;n=1;}
                            count+=n;if(count>item.size)throw new IOException("Mod data exceeds its ZIP size");
                            crc.update(buffer,0,n);output.write(buffer,0,n);written+=n;progress.update(written,total,"Saving mod data");
                        }
                        output.getFD().sync();
                    }
                    if(count!=item.size||crc.getValue()!=item.crc)throw new IOException("Damaged mod data: "+item.path);
                }
                check(cancelled);
                if(!stage.renameTo(dest))throw new IOException("Cannot finish mod installation");
                done=true;committed=true;
            }finally{if(!done)delete(stage);}
        }
        @Override public void close() throws IOException {
            try{if(zip!=null)zip.close();}finally{Files.deleteIfExists(archive.toPath());}
        }
    }
    Import open(InputStream source,long expected,BooleanSupplier cancelled,Progress progress) throws IOException {
        if(source==null)throw new IOException("Cannot open the selected ZIP");
        if(expected>MAX_ZIP)throw new IOException("Mod ZIP is larger than 4 GiB");
        Import result=new Import(cancelled);boolean success=false;
        try{
            byte[] buffer=new byte[131072];long count=0;
            try(FileOutputStream output=new FileOutputStream(result.archive)){
                for(int n;(n=source.read(buffer))!=-1;){
                    check(cancelled);if(n==0){int one=source.read();if(one<0)break;buffer[0]=(byte)one;n=1;}
                    count+=n;if(count>MAX_ZIP)throw new IOException("Mod ZIP is larger than 4 GiB");
                    output.write(buffer,0,n);progress.update(count,expected,"Reading mod ZIP");
                }
            }
            checkFormat(result.archive);
            if(count<22||expected>=0&&expected!=count)throw new IOException("Incomplete mod ZIP. Download it again or create a ZIP of the extracted mod files.");
            check(cancelled);
            try{result.zip=new ZipFile(result.archive,Charset.forName("CP437"));}
            catch(ZipException error){throw new IOException("Could not read this ZIP. It may be incomplete or use another archive format. Extract the mod files and create a new ZIP.",error);}
            Set<String> paths=new HashSet<>(),files=new HashSet<>();long total=0;int entryCount=0;
            for(java.util.Enumeration<? extends ZipEntry> all=result.zip.entries();all.hasMoreElements();){
                check(cancelled);ZipEntry entry=all.nextElement();
                if(++entryCount>MAX_FILES)throw new IOException("Mod ZIP has too many entries");
                String name=path(entry.getName()),lower=name.toLowerCase(Locale.ROOT);
                if(!paths.add(lower))throw new IOException("Duplicate ZIP path: "+name);
                if(entry.isDirectory())continue;
                if(entry.getSize()<0||entry.getSize()>MAX_FILE||entry.getCrc()<0||
                    (entry.getMethod()!=ZipEntry.STORED&&entry.getMethod()!=ZipEntry.DEFLATED))throw new IOException("Unsupported ZIP entry: "+name);
                total+=entry.getSize();if(total>MAX_TOTAL)throw new IOException("Mod expands beyond 8 GiB");
                files.add(lower);int slot=result.items.size();
                result.items.add(new Item(slot,name,entry.getSize(),entry.getCrc()));result.entries.add(entry);
            }
            for(String name:files){int at=0;while((at=name.indexOf('/',at))>=0){if(files.contains(name.substring(0,at)))throw new IOException("Conflicting ZIP paths");at++;}}
            if(result.items.isEmpty())throw new IOException("The mod ZIP has no files");
            success=true;return result;
        }finally{if(!success)result.close();}
    }
    byte[] read(String id,int slot,long offset,int length) throws IOException {
        if(!id(id)||slot<0||slot>=MAX_FILES)throw new IOException("Invalid mod data request");
        File file=new File(new File(root,id),slot+".bin");
        try(RandomAccessFile input=new RandomAccessFile(file,"r")){
            FileChunks.validate(input.length(),offset,length);
            int count=(int)Math.min(length,input.length()-offset);byte[] bytes=new byte[count];input.seek(offset);input.readFully(bytes);return bytes;
        }
    }
    File mediaFile(String id,int slot) throws IOException {
        if(!id(id)||slot<0||slot>=MAX_FILES)throw new IOException("Invalid mod media request");
        File file=new File(new File(root,id),slot+".bin");
        if(!file.isFile())throw new IOException("Installed mod media is missing");
        return file;
    }
    static long[] mediaRange(String header,long size) throws IOException {
        long from=0,to=size-1;
        if(size<=0)throw new IOException("Empty mod media");
        if(header!=null){
            if(!header.matches("bytes=(?:[0-9]+-[0-9]*|-[0-9]+)"))throw new IOException("Invalid media range");
            String[] parts=header.substring(6).split("-",-1);
            try{
                if(parts[0].isEmpty()){long suffix=Long.parseLong(parts[1]);if(suffix<=0)throw new IOException("Invalid media range");from=Math.max(0,size-suffix);}
                else{from=Long.parseLong(parts[0]);if(!parts[1].isEmpty())to=Math.min(to,Long.parseLong(parts[1]));}
            }catch(NumberFormatException error){throw new IOException("Invalid media range",error);}
            if(from<0||from>=size||to<from)throw new IOException("Invalid media range");
        }
        return new long[]{from,to-from+1};
    }
    static InputStream mediaStream(File file,long start,long length) throws IOException {
        java.io.FileInputStream source=new java.io.FileInputStream(file);
        try{source.getChannel().position(start);}catch(IOException error){source.close();throw error;}
        return new java.io.FilterInputStream(source){
            long remaining=length;
            @Override public int read() throws IOException {if(remaining<=0)return -1;int value=super.read();if(value>=0)remaining--;return value;}
            @Override public int read(byte[] data,int offset,int count) throws IOException {if(count==0)return 0;if(remaining<=0)return -1;int n=in.read(data,offset,(int)Math.min(remaining,count));if(n>0)remaining-=n;return n;}
            @Override public long skip(long count) throws IOException {long n=in.skip(Math.max(0,Math.min(remaining,count)));remaining-=n;return n;}
            @Override public int available() throws IOException {return (int)Math.min(remaining,in.available());}
        };
    }
    void remove(String id) throws IOException {
        if(!id(id))throw new IOException("Invalid mod identifier");delete(new File(root,id));
    }
    void cleanup(Set<String> keep,String importing) throws IOException {
        for(String id:keep)if(!id(id))throw new IOException("Invalid mod library identifier");
        File[] files=root.listFiles();if(files==null)throw new IOException("Cannot read mod storage");
        for(File file:files){String name=file.getName();
            if(id(name)&&!keep.contains(name)&&!name.equals(importing)||name.matches("stage-pack-[a-f0-9]{32}")&&!name.equals("stage-"+importing))delete(file);
        }
        files=cache.listFiles();if(files==null)throw new IOException("Cannot read mod staging storage");
        for(File file:files)if(file.getName().matches("pack-[a-f0-9]{32}\\.zip")&&!file.getName().equals(importing+".zip"))delete(file);
    }
    private static void delete(File file) throws IOException {
        if(!file.exists())return;
        if(file.isDirectory()){
            File[] children=file.listFiles();if(children==null)throw new IOException("Cannot read mod storage");
            for(File child:children)delete(child);
        }
        if(!file.delete())throw new IOException("Cannot remove mod storage");
    }
}
