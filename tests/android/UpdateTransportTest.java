package org.re1.launcher;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

public final class UpdateTransportTest {
    static class Fake extends HttpURLConnection {
        final int status;final byte[] data;String location;long declared=-1;
        boolean closed,disconnected;
        Fake(int status,byte[] data)throws Exception{super(new URL(UpdateTransport.ENDPOINT));this.status=status;this.data=data;}
        public void connect(){} public boolean usingProxy(){return false;}
        public void disconnect(){disconnected=true;}
        public int getResponseCode(){return status;}
        public long getContentLengthLong(){return declared;}
        public String getHeaderField(String name){return "Location".equals(name)?location:null;}
        public InputStream getInputStream(){return new ByteArrayInputStream(data){public void close(){closed=true;}};}
    }
    static void check(boolean value){if(!value)throw new AssertionError();}
    static void rejected(UpdateTransport.Connections connection)throws Exception{
        try{UpdateTransport.latest(connection);throw new AssertionError("Expected rejection");}catch(IOException expected){}
    }
    public static void main(String[] args)throws Exception{
        Fake ok=new Fake(200,"{\"tag_name\":\"v1.2\"}".getBytes(StandardCharsets.UTF_8));
        UpdateTransport.Result result=UpdateTransport.latest(url->{check(url.toString().equals(UpdateTransport.ENDPOINT));return ok;});
        check(result.status==200&&result.body.contains("v1.2")&&ok.closed&&ok.disconnected);
        check(ok.getConnectTimeout()==4000&&ok.getReadTimeout()==4000&&!ok.getInstanceFollowRedirects());
        check("ResidentEvil1996Android/1.1".equals(ok.getRequestProperty("User-Agent")));
        for(int status:new int[]{404,403,429,500}){
            Fake fail=new Fake(status,new byte[0]);
            check(UpdateTransport.latest(url->fail).status==status&&fail.disconnected);
        }
        for(long length:new long[]{-1,UpdateTransport.MAX_BYTES+1}){
            Fake huge=new Fake(200,new byte[UpdateTransport.MAX_BYTES+1]);huge.declared=length;
            rejected(url->huge);check(huge.disconnected);
        }
        for(String location:new String[]{"https://example.com/evil","http://api.github.com/repos/x", "https://api.github.com:444/repos/x", "https://user@api.github.com/repos/x", "https://api.github.com/not-repos"}){
            Fake redirect=new Fake(302,new byte[0]);redirect.location=location;
            ArrayList<URL> opened=new ArrayList<>();
            rejected(url->{opened.add(url);return redirect;});
            check(opened.size()==1&&redirect.disconnected);
        }
        Fake redirect=new Fake(301,new byte[0]);redirect.location="https://api.github.com/repos/NewName/Android/releases/latest";
        int[] calls={0};
        check(UpdateTransport.latest(url->calls[0]++==0?redirect:ok).status==200&&calls[0]==2);
        rejected(url->redirect);
        System.out.println("UpdateTransportTest passed: bounded metadata, failures, redirects and cleanup");
    }
}
