int main(int argc, char *argv[])
{
   XrdOucEnv myEnv;
   XrdOucStream Config(&m_log, getenv("XRDINSTANCE"), &myEnv, "=====> ");

   // Obtain plugin configurator
   XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(config_filename,&Config,&m_log,
                                                &XrdVERSIONINFOVAR(XrdOucGetCache));

   if (ofsCfg->Load(XrdOfsConfigPI::theOssLib)) 
      ofsCfg->Plugin(m_output_fs);


   int resc =   output_fs.Create(Factory::GetInstance().RefConfiguration().m_username.c_str(), m_temp_filename.c_str(), 0644, myEnv, XRDOSS_mkpath);

   m_output = output_fs.newFile(Factory::GetInstance().RefConfiguration().m_username.c_str());
   int res = m_output->Open(m_temp_filename.c_str(), O_RDWR, 0644, myEnv);

    XrdFileCacheInfo cfi;
    cfi.Read(m_infoFile);
    printf("Info is complete %d\n", cfi.IsComplete());
}
