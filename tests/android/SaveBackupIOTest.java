package org.re1.launcher;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Arrays;

public final class SaveBackupIOTest {
    interface Attempt {void run() throws IOException;}
    static void rejects(Attempt attempt) throws IOException {
        try{attempt.run();throw new AssertionError("Expected rejection");}catch(IOException expected){}
    }
    public static void main(String[] args) throws Exception {
        byte[] bytes=new byte[22000];for(int i=0;i<bytes.length;i++)bytes[i]=(byte)(i*37+11);
        ByteArrayOutputStream output=new ByteArrayOutputStream();SaveBackupIO.writeBytes(output,bytes);
        if(!Arrays.equals(bytes,SaveBackupIO.readBytes(new ByteArrayInputStream(output.toByteArray()),SaveBackupIO.MAX_BYTES)))throw new AssertionError("Binary bytes changed");
        byte[] max=new byte[SaveBackupIO.MAX_BYTES];
        if(SaveBackupIO.readBytes(new ByteArrayInputStream(max),max.length).length!=max.length)throw new AssertionError("Boundary rejected");
        rejects(()->SaveBackupIO.readBytes(new ByteArrayInputStream(new byte[max.length+1]),max.length));
        rejects(()->SaveBackupIO.writeBytes(new ByteArrayOutputStream(),new byte[max.length+1]));
        rejects(()->SaveBackupIO.readBytes(null,max.length));rejects(()->SaveBackupIO.writeBytes(null,bytes));
        ByteArrayInputStream unusual=new ByteArrayInputStream(bytes){
            int calls;
            @Override public int read(byte[] b,int off,int len){return ++calls%3==0?0:super.read(b,off,Math.min(len,2));}
        };
        if(!Arrays.equals(bytes,SaveBackupIO.readBytes(unusual,max.length)))throw new AssertionError("Short reads lost bytes");
        System.out.println("PASS: native ZIP bytes, size bounds, null providers and short/zero reads");
    }
}
