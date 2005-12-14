class XrdClientAdminJNI {
   // This usually is an xrootd redirector
   private String firsturl;


   private native Boolean chmod(String locpathfile1, int user, int group, int other);

   private native Boolean dirlist(String path, String[] lst);

   private native Boolean existfiles(String[] pathslist, Boolean[] res);

   private native Boolean existdirs(String[] pathslist, Boolean[] res);

   private native Boolean getchecksum(String pathname, String chksum);

   private native Boolean isfileonline(String[] pathslist, Boolean[] res);

   // Finds one of the final locations for a given file
   // Returns false if errors occurred
   private native Boolean locate(String pathfile, String hostname);

   private native Boolean mv(String locpathfile1, String locpathfile2);

   private native Boolean mkdir(String locpathfile1, int user, int group, int other);

   private native Boolean rm(String locpathfile);

   private native Boolean rmdir(String locpath);

   private native Boolean prepare(String[] pathnamelist, char opts, char priority);

   // Gives info for a given file
   private native Boolean stat(String pathfile, int id, long size, int flags, int modtime);



   private XrdClientAdminJNI(String hostname) { firsturl = hostname; };

  public static void main(String args[]) {
    XrdClientAdminJNI a = new XrdClientAdminJNI("xroot://kanolb-a.slac.stanford.edu//dummy");
    String newhost = "";
    boolean r = a.locate("pippo.root", newhost);
    System.out.println("Locate Result: " + r + " host: '" + newhost + "'");
  }

  static {
    System.loadLibrary("XrdClientAdminJNI");
  }
}
