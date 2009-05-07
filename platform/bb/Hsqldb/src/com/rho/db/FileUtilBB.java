package com.rho.db;

import j2me.io.File;
//import j2me.io.FileDescriptor;
//import j2me.io.FileInputStream;
//import j2me.io.FileOutputStream;
import java.io.IOException;
import java.util.Random;

import javax.microedition.io.file.FileConnection;
import javax.microedition.io.Connector;

import org.hsqldb.lib.FileAccess;

/**
 * A collection of static file management methods.<p>
 * Also implements the default FileAccess method
 */
public class FileUtilBB implements FileAccess {

    private static FileUtilBB fileUtil = new FileUtilBB();

    FileUtilBB() {}

    public static FileUtilBB getDefaultInstance() {
        return fileUtil;
    }

    public boolean isStreamElement(java.lang.String elementName) {
    	return exists(elementName);
    }

    public long getFileLength(java.lang.String filename){
    	
    	try{
	    	FileConnection fconn = (FileConnection)Connector.open(filename);
	    	return fconn.fileSize();
    	}catch(IOException exc){
    		System.out.println("FileUtilBB:getFileLength Exception: " + exc.getMessage());
    	}
    	
    	return 0;
    }
    
    public void deleteOnExit(java.lang.String filename){
    	//TODO: deleteOnExit
    }
    
//    public java.io.InputStream openInputStreamElement( java.lang.String streamName) throws IOException {
    	//TODO:openInputStreamElement
/*
        try {
            return new FileInputStream(new File(streamName));
        } catch (Throwable e) {
            throw toIOException(e);
        }*/
    //	return null;
    //}

    public void createParentDirs(java.lang.String filename) {
        //makeParentDirectories(new File(filename));
    	//TODO:createParentDirs
    }

    public void removeElement(java.lang.String filename) {

        if (isStreamElement(filename)) {
            delete(filename);
        }
    }

    public void renameElement(java.lang.String oldName,
                              java.lang.String newName) {
        renameOverwrite(oldName, newName);
    }

    public java.io.OutputStream openOutputStreamElement(
            java.lang.String streamName) throws java.io.IOException {
    	//TODO:openOutputStreamElement
        //return new FileOutputStream(new File(streamName));
    	return null;
    }

    // end of FileAccess implementation
    // a new File("...")'s path is not canonicalized, only resolved
    // and normalized (e.g. redundant separator chars removed),
    // so as of JDK 1.4.2, this is a valid test for case insensitivity,
    // at least when it is assumed that we are dealing with a configuration
    // that only needs to consider the host platform's native file system,
    // even if, unlike for File.getCanonicalPath(), (new File("a")).exists() or
    // (new File("A")).exits(), regardless of the hosting system's
    // file path case sensitivity policy.
    public final boolean fsIsIgnoreCase = true;
//        (new File("A")).equals(new File("a"));

    // posix separator normalized to File.separator?
    // CHECKME: is this true for every file system under Java?
    public final boolean fsNormalizesPosixSeparator = true;
 //       (new File("/")).getPath().endsWith(File.separator);

    // for JDK 1.1 createTempFile
    final Random random = new Random(System.currentTimeMillis());

    /**
     * Delete the named file
     */
    public void delete(String filename) 
    {
        FileConnection fconn = null;
    	
    	try{
    		fconn = (FileConnection)Connector.open(filename,Connector.READ_WRITE);
    		//if ( fc.isDirectory() )
    		//	deleteFilesInFolder(fc);
        	
    		fconn.delete();
    	}catch(IOException exc){
    		System.out.println("FileUtilBB:delete '" + filename + "' Exception: " + exc.getMessage());
    	}finally{
    		if ( fconn != null )
    			try{fconn.close();}catch(IOException exc){
    				System.out.println("FileUtilBB:delete close '" + filename + "' Exception: " + exc.getMessage());
			    }
    	}
    	
    }

    /**
     * Requests, in a JDK 1.1 compliant way, that the file or directory denoted
     * by the given abstract pathname be deleted when the virtual machine
     * terminates. <p>
     *
     * Deletion will be attempted only for JDK 1.2 and greater runtime
     * environments and only upon normal termination of the virtual
     * machine, as defined by the Java Language Specification. <p>
     *
     * Once deletion has been sucessfully requested, it is not possible to
     * cancel the request. This method should therefore be used with care. <p>
     *
     * @param f the abstract pathname of the file be deleted when the virtual
     *       machine terminates
     */
    public void deleteOnExit(File f) {
        //JavaSystem.deleteOnExit(f);
    }

    /**
     * Return true or false based on whether the named file exists.
     */
    public boolean exists(String filename) 
    {
        FileConnection fconn = null;
    	
    	try{
        	fconn = (FileConnection)Connector.open(filename);
        	return fconn.exists();
    	}catch(IOException exc){
    		System.out.println("FileUtilBB:exists '" + filename + "' Exception: " + exc.getMessage());
    	}finally{
    		if ( fconn != null )
    			try{fconn.close();}catch(IOException exc){
    				System.out.println("FileUtilBB:exists close '" + filename + "' Exception: " + exc.getMessage());
			    }
    	}
    	
    	return false;
    }

    public boolean exists(String fileName, boolean resource, Class cla) {

        if (fileName == null || fileName.length() == 0) {
            return false;
        }

        return resource ? null != cla.getResourceAsStream(fileName) : exists(fileName);
    }

    /**
     * Rename the file with oldname to newname. If a file with newname already
     * exists, it is deleted before the renaming operation proceeds.
     *
     * If a file with oldname does not exist, no file will exist after the
     * operation.
     */
    private void renameOverwrite(String oldname, String newname) {

        delete(newname);

        FileConnection fconn = null;
    	try{
        	fconn = (FileConnection)Connector.open(oldname, Connector.READ_WRITE);
        	
        	String name = newname;
        	int nSlash = newname.lastIndexOf('/');
        	if ( nSlash >= 0 )
        		name = newname.substring(nSlash+1);
        	
        	if ( fconn.exists() ){
        		fconn.rename(name);
        	}
    	}catch(IOException exc){
    		System.out.println("FileUtilBB:renameOverwrite from '" + oldname + "' to '" + newname + "' Exception: " + exc.getMessage());
    	}finally{
    		if ( fconn != null )
    			try{fconn.close();}catch(IOException exc){
    				System.out.println("FileUtilBB:renameOverwrite close Exception: " + exc.getMessage());
			    }
    	}
    }

    public static IOException toIOException(Throwable e) {

        if (e instanceof IOException) {
            return (IOException) e;
        } else {
            return new IOException(e.toString());
        }
    }

    /**
     * Retrieves the absolute path, given some path specification.
     *
     * @param path the path for which to retrieve the absolute path
     * @return the absolute path
     */
    public String absolutePath(String path) {
    	//TODO: absolutePath
        return path;
    }

    /**
     * Retrieves the canonical file for the given file, in a
     * JDK 1.1 complaint way.
     *
     * @param f the File for which to retrieve the absolute File
     * @return the canonical File
     */
    public File canonicalFile(File f) throws IOException {
    	//TODO: canonicalFile
        return f;
    }

    /**
     * Retrieves the canonical file for the given path, in a
     * JDK 1.1 complaint way.
     *
     * @param path the path for which to retrieve the canonical File
     * @return the canonical File
     */
    public File canonicalFile(String path) throws IOException {
    	//TODO: canonicalFile
        return new File(path);
    }

    /**
     * Retrieves the canonical path for the given File, in a
     * JDK 1.1 complaint way.
     *
     * @param f the File for which to retrieve the canonical path
     * @return the canonical path
     */
    public String canonicalPath(File f) throws IOException {
    	//TODO: canonicalPath
        return f.getPath();
    }

    /**
     * Retrieves the canonical path for the given path, in a
     * JDK 1.1 complaint way.
     *
     * @param path the path for which to retrieve the canonical path
     * @return the canonical path
     */
    public String canonicalPath(String path) throws IOException {
    	//TODO: canonicalPath
        return path;
    }

    /**
     * Retrieves the canonical path for the given path, or the absolute
     * path if attemting to retrieve the canonical path fails.
     *
     * @param path the path for which to retrieve the canonical or
     *      absolute path
     * @return the canonical or absolute path
     */
    public String canonicalOrAbsolutePath(String path) {

        try {
            return canonicalPath(path);
        } catch (Exception e) {
            return absolutePath(path);
        }
    }

    public void makeParentDirectories(File f) {
/*
        String parent = f.getParent();

        if (parent != null) {
            new File(parent).mkdirs();
        } else {

            // workaround for jdk 1.1 bug (returns null when there is a parent)
            parent = f.getPath();

            int index = parent.lastIndexOf('/');

            if (index > 0) {
                parent = parent.substring(0, index);

                new File(parent).mkdirs();
            }
        }*/
    	//TODO: makeParentDirectories 
    }
/*
    public class FileSync implements FileAccess.FileSync {

        //FileDescriptor outDescriptor;

//        FileSync(FileOutputStream os) throws java.io.IOException {
            //outDescriptor = os.getFD();
//        }

        public void sync() throws java.io.IOException {
            //outDescriptor.sync();
        }
    }

    public FileAccess.FileSync getFileSync(java.io.OutputStream os) throws java.io.IOException 
    {
        return new FileSync();//(FileOutputStream) os);
    }*/
}