package org.re1.launcher;

import java.io.ByteArrayInputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

/** Runs the same reader packaged in the APK, without Android framework mocks. */
public final class FileChunksTest {
    private static final byte[] MOVIE = new byte[1535452];
    static { for(int n=0;n<MOVIE.length;n++)MOVIE[n]=(byte)(n*37+11); }

    private static void check(InputStream source, int offset, int count) throws IOException {
        try(source) {
            byte[] actual=FileChunks.read(source,MOVIE.length,offset,count);
            if(!Arrays.equals(actual,Arrays.copyOfRange(MOVIE,offset,offset+count)))
                throw new AssertionError("Wrong bytes at "+offset+" length "+count);
        }
    }
    private interface Attempt { void run() throws IOException; }
    private static void rejects(Attempt attempt, Class<? extends IOException> expected) throws IOException {
        try {attempt.run();throw new AssertionError("Expected "+expected.getSimpleName());}
        catch(IOException error) {if(!expected.isInstance(error))throw error;}
    }
    private static InputStream provider(boolean canSkip) {
        return new ByteArrayInputStream(MOVIE) {
            int calls;
            @Override public long skip(long n) {return canSkip?super.skip(Math.min(n,4093)):0;}
            @Override public int read(byte[] b,int off,int len) {
                if(++calls%13==0)return 0;
                return super.read(b,off,Math.min(len,137));
            }
        };
    }
    public static void main(String[] args) throws Exception {
        // Same offset, total size and partial final chunk as the phone report.
        check(new ByteArrayInputStream(MOVIE),1310720,224732);
        check(provider(false),1310720,224732);
        check(provider(true),1310720,224732);
        for(int start=0;start<MOVIE.length;start+=FileChunks.MAX_LENGTH)
            check(provider(true),start,Math.min(FileChunks.MAX_LENGTH,MOVIE.length-start));
        check(provider(false),MOVIE.length-1,1);
        rejects(()->FileChunks.read(new ByteArrayInputStream(new byte[32]),MOVIE.length,1310720,224732),EOFException.class);
        rejects(()->FileChunks.read(new ByteArrayInputStream(Arrays.copyOf(MOVIE,MOVIE.length-1)),MOVIE.length,1310720,224732),EOFException.class);
        for(long offset:new long[]{-1,MOVIE.length,Long.MAX_VALUE})
            rejects(()->FileChunks.validate(MOVIE.length,offset,1),IOException.class);
        for(int count:new int[]{-1,0,FileChunks.MAX_LENGTH+1,Integer.MAX_VALUE})
            rejects(()->FileChunks.validate(MOVIE.length,0,count),IOException.class);
        rejects(()->FileChunks.read(new InputStream(){public int read()throws IOException{throw new IOException("Provider disconnected");}},MOVIE.length,0,1),IOException.class);
        System.out.println("PASS: APK file reader: reported movie tail, every chunk/binary byte, partial/zero reads, non-seekable providers, truncated files, provider exceptions and invalid ranges");
    }
}
