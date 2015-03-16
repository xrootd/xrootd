from XrdTest.ClusterUtils import Cluster, Network, Host, Disk

def getCluster():
    cluster = Cluster()
    #---------------------------------------------------------------------------
    # Global names
    #---------------------------------------------------------------------------
    cluster.name = 'cluster_pyxrootd'
    network_name = cluster.name + '_net'

    #---------------------------------------------------------------------------
    # Cluster defaults
    #
    # The bootImage parameter is relative to some libvirt-managed storage pool.
    #---------------------------------------------------------------------------
    cluster.defaultHost.bootImage = 'slc6_testslave_ref.img'
    cluster.defaultHost.cacheBootImage = True
    cluster.defaultHost.arch = 'x86_64'
    cluster.defaultHost.ramSize = '1048576'
    cluster.defaultHost.net = network_name

    #---------------------------------------------------------------------------
    # Network definition
    #---------------------------------------------------------------------------
    net = Network()
    net.bridgeName = 'virbr_cl_pyxrd'
    net.name = network_name
    net.ip = '192.168.10.1'
    net.netmask = '255.255.255.0'
    net.DHCPRange = ('192.168.10.2', '192.168.10.254')

    #---------------------------------------------------------------------------
    # Host definitions
    #---------------------------------------------------------------------------
    manager = Host('manager.xrd.test', '192.168.10.10')
    client1 = Host('client1.xrd.test', '192.168.10.11')
    client2 = Host('client2.xrd.test', '192.168.10.12')
    client3 = Host('client3.xrd.test', '192.168.10.13')
    client4 = Host('client4.xrd.test', '192.168.10.14')
    client5 = Host('client5.xrd.test', '192.168.10.15')
    client6 = Host('client6.xrd.test', '192.168.10.16')
    client7 = Host('client7.xrd.test', '192.168.10.17')
    client8 = Host('client8.xrd.test', '192.168.10.18')

    # Hosts to be included in the cluster
    hosts = [manager, client1, client2, client3, client4, client5, client6,
             client7, client8]

    #---------------------------------------------------------------------------
    # Additional host disk definitions
    #
    # As per the libvirt docs, the device name given here is not guaranteed to
    # map to the same name in the guest OS. Incrementing the device name works
    # (i.e. disk1 = vda, disk2 = vdb etc.).
    #
    # Disk sizes should be larger than 10GB for data server nodes, otherwise
    # the node might not be selected by the cmsd.
    #---------------------------------------------------------------------------
    manager.disks =  [Disk('disk1', '20G', device='vda', mountPoint='/data')]
    client1.disks =  [Disk('disk1', '10G', device='vda', mountPoint='/data')]
    client2.disks =  [Disk('disk1', '10G', device='vda', mountPoint='/data')]
    client3.disks =  [Disk('disk1', '10G', device='vda', mountPoint='/data')]
    client4.disks =  [Disk('disk1', '10G', device='vda', mountPoint='/data')]
    client5.disks =  [Disk('disk1', '10G', device='vda', mountPoint='/data')]
    client6.disks =  [Disk('disk1', '10G', device='vda', mountPoint='/data')]
    client7.disks =  [Disk('disk1', '10G', device='vda', mountPoint='/data')]
    client8.disks =  [Disk('disk1', '10G', device='vda', mountPoint='/data')]

    #---------------------------------------------------------------------------
    # Optional load balancing configuration
    #---------------------------------------------------------------------------
    # The DNS alias to be used
#     net.lbAlias = 'lb.xrd.test'
#     # The machines that will be load balanced (round-robin) under the alias
#     net.lbHosts = [ds1, ds2, ds3, ds4]

    net.addHosts(hosts)
    cluster.network = net
    cluster.addHosts(hosts)
    return cluster

