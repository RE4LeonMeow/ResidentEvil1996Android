package org.re1.launcher;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/** Port-owned, bounded public release-metadata request. Never downloads an APK. */
final class UpdateTransport {
    static final String ENDPOINT = "https://api.github.com/repos/RE4LeonMeow/ResidentEvil1996Android/releases/latest";
    static final String RELEASES = "https://github.com/RE4LeonMeow/ResidentEvil1996Android/releases";
    static final int MAX_BYTES = 256 * 1024;
    interface Connections { HttpURLConnection open(URL url) throws IOException; }
    static final class Result {
        final int status;
        final String body;
        Result(int status, String body) { this.status=status; this.body=body; }
    }
    static Result latest() throws IOException {
        return latest(url -> (HttpURLConnection)url.openConnection());
    }
    static Result latest(Connections connections) throws IOException {
        URL url=new URL(ENDPOINT);
        for(int hop=0;hop<3;hop++) {
            if(!allowed(url))throw new IOException("Unexpected release metadata destination");
            HttpURLConnection connection=connections.open(url);
            try {
                connection.setInstanceFollowRedirects(false);
                connection.setConnectTimeout(4000);connection.setReadTimeout(4000);
                connection.setRequestMethod("GET");
                connection.setRequestProperty("User-Agent","ResidentEvil1996Android/1.1");
                connection.setRequestProperty("Accept","application/vnd.github+json");
                connection.setRequestProperty("X-GitHub-Api-Version","2022-11-28");
                int status=connection.getResponseCode();
                if(status==301||status==302||status==307||status==308) {
                    String location=connection.getHeaderField("Location");
                    if(location==null)throw new IOException("Missing release redirect");
                    url=new URL(url,location);continue;
                }
                if(status!=200)return new Result(status,"");
                if(connection.getContentLengthLong()>MAX_BYTES)throw new IOException("Release metadata too large");
                try(InputStream input=connection.getInputStream();ByteArrayOutputStream output=new ByteArrayOutputStream()) {
                    byte[] buffer=new byte[8192];int count;
                    while((count=input.read(buffer))!=-1) {
                        if(output.size()>MAX_BYTES-count)throw new IOException("Release metadata too large");
                        output.write(buffer,0,count);
                    }
                    return new Result(200,new String(output.toByteArray(),StandardCharsets.UTF_8));
                }
            } finally { connection.disconnect(); }
        }
        throw new IOException("Too many release redirects");
    }
    private static boolean allowed(URL url) {
        return "https".equals(url.getProtocol())&&"api.github.com".equalsIgnoreCase(url.getHost())
            &&(url.getPort()==-1||url.getPort()==443)&&url.getUserInfo()==null
            &&url.getPath().startsWith("/repos/");
    }
}
