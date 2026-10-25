package org.re1.launcher;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/** Bounded binary document-provider I/O for save ZIPs. */
final class SaveBackupIO {
    static final int MAX_BYTES=131072;
    static byte[] readBytes(InputStream input,int limit) throws IOException {
        if(input==null)throw new IOException("Could not open save backup.");
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();
        byte[] chunk=new byte[4096];int count;
        while((count=input.read(chunk))!=-1){
            if(count==0){int next=input.read();if(next<0)break;bytes.write(next);}
            else bytes.write(chunk,0,count);
            if(bytes.size()>limit)throw new IOException("Save file is too large.");
        }
        return bytes.toByteArray();
    }
    static void writeBytes(OutputStream output,byte[] bytes) throws IOException {
        if(output==null)throw new IOException("Could not create save backup.");
        if(bytes.length>MAX_BYTES)throw new IOException("Save backup is too large.");
        output.write(bytes);output.flush();
    }
}
