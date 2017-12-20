
#include <stdio.h>
#include <dpdk/dpdk.h>

struct ether_addr next_dst[2];
struct ether_addr next_src[2];

constexpr size_t n_queues = 1;
int l2fwd(void*)
{
  const size_t n_ports = rte_eth_dev_count();
  while (true) {
    for (size_t pid=0; pid<n_ports; pid++) {
			for (size_t qid=0; qid<n_queues; qid++) {
				constexpr size_t BURSTSZ = 32;
				rte_mbuf* mbufs[BURSTSZ];

				size_t nb_recv = rte_eth_rx_burst(pid, qid, mbufs, BURSTSZ);
				if (nb_recv == 0) continue;

        for (size_t i=0; i<nb_recv; i++) {
          uint8_t* p = rte_pktmbuf_mtod(mbufs[i], uint8_t*);
          memcpy(p+0, &next_dst[pid], 6);
          memcpy(p+6, &next_src[pid], 6);
        }
#if 1
        printf("recvfrom p%zd src=%s dst=%s\n", pid,
            dpdk::ether_addr2str(&next_src[pid]).c_str(),
            dpdk::ether_addr2str(&next_dst[pid]).c_str());
#endif
        size_t nb_send = rte_eth_tx_burst(pid^1, qid, mbufs, nb_recv);
        if (nb_send < nb_recv) {
          dpdk::rte_pktmbuf_free_bulk(&mbufs[nb_send], nb_recv-nb_send);
        }
			}
    }
  }
}

int main(int argc, char** argv)
{

  dpdk::dpdk_boot(argc, argv);
  struct rte_eth_conf port_conf;
  dpdk::init_portconf(&port_conf);
  struct rte_mempool* mp = dpdk::mp_alloc("RXMBUFMP", 0, 8192);

  size_t n_ports = rte_eth_dev_count();
  if (n_ports == 0) throw dpdk::exception("no ethdev");
  printf("%zd ports found \n", n_ports);
  for (size_t i=0; i<n_ports; i++) {
    dpdk::port_configure(i, n_queues, n_queues, &port_conf, mp);
  }

  next_dst[0].addr_bytes[0] = 0x52;
  next_dst[0].addr_bytes[1] = 0x54;
  next_dst[0].addr_bytes[2] = 0x00;
  next_dst[0].addr_bytes[3] = 0x33;
  next_dst[0].addr_bytes[4] = 0x33;
  next_dst[0].addr_bytes[5] = 0x33;

  next_dst[1].addr_bytes[0] = 0xa0;
  next_dst[1].addr_bytes[1] = 0x36;
  next_dst[1].addr_bytes[2] = 0x9f;
  next_dst[1].addr_bytes[3] = 0x39;
  next_dst[1].addr_bytes[4] = 0x10;
  next_dst[1].addr_bytes[5] = 0x4c;

  rte_eth_macaddr_get(0, &next_src[1]);
  rte_eth_macaddr_get(1, &next_src[0]);

  printf("flow[0->1]: %s -> %s \n",
      dpdk::ether_addr2str(&next_src[0]).c_str(),
      dpdk::ether_addr2str(&next_dst[0]).c_str());
  printf("flow[1->0]: %s -> %s \n",
      dpdk::ether_addr2str(&next_src[1]).c_str(),
      dpdk::ether_addr2str(&next_dst[1]).c_str());

  rte_eal_remote_launch(l2fwd, nullptr, 1);
  rte_eal_mp_wait_lcore();
}

