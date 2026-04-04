#include <u.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <ifaddrs.h>
#include <libc.h>
#include <ip.h>

/*
 * Use getifaddrs to find interfaces (macOS/Darwin).
 */

static Ipifc*
findifc(Ipifc **list, char *name)
{
	Ipifc *ifc;

	for(ifc=*list; ifc; ifc=ifc->next)
		if(strcmp(ifc->dev, name) == 0)
			return ifc;

	ifc = mallocz(sizeof *ifc, 1);
	if(ifc == nil)
		return nil;
	strecpy(ifc->dev, ifc->dev+sizeof ifc->dev, name);

	while(*list)
		list = &(*list)->next;
	*list = ifc;
	return ifc;
}

Ipifc*
readipifc(char *net, Ipifc *ifc, int index)
{
	struct ifaddrs *ifap, *ifa;
	Ipifc *list, *nifc;
	Iplifc *lifc, **l;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sockaddr_dl *sdl;
	int idx;

	USED(net);
	freeipifc(ifc);
	list = nil;

	if(getifaddrs(&ifap) != 0)
		return nil;

	for(ifa=ifap; ifa; ifa=ifa->ifa_next){
		if(ifa->ifa_addr == nil)
			continue;

		idx = if_nametoindex(ifa->ifa_name);
		if(index >= 0 && idx != index)
			continue;

		nifc = findifc(&list, ifa->ifa_name);
		if(nifc == nil)
			continue;
		nifc->index = idx;

		switch(ifa->ifa_addr->sa_family){
		case AF_LINK:
			sdl = (struct sockaddr_dl*)ifa->ifa_addr;
			if(sdl->sdl_type == IFT_ETHER && sdl->sdl_alen == 6)
				memmove(nifc->ether, LLADDR(sdl), 6);
			{
				int fd;
				struct ifreq ifr;

				fd = socket(AF_INET, SOCK_DGRAM, 0);
				if(fd >= 0){
					memset(&ifr, 0, sizeof ifr);
					strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
					if(ioctl(fd, SIOCGIFMTU, &ifr) >= 0){
						nifc->mtu = ifr.ifr_mtu;
						nifc->rp.linkmtu = ifr.ifr_mtu;
					}
					close(fd);
				}
			}
			break;

		case AF_INET:
			sin = (struct sockaddr_in*)ifa->ifa_addr;
			lifc = mallocz(sizeof *lifc, 1);
			if(lifc == nil)
				break;
			memmove(lifc->ip, v4prefix, IPv4off);
			memmove(lifc->ip+IPv4off, &sin->sin_addr, IPv4addrlen);

			if(ifa->ifa_netmask){
				struct sockaddr_in *nm;
				nm = (struct sockaddr_in*)ifa->ifa_netmask;
				memmove(lifc->mask, v4prefix, IPv4off);
				memset(lifc->mask, 0xff, IPv4off);
				memmove(lifc->mask+IPv4off, &nm->sin_addr, IPv4addrlen);
			}
			maskip(lifc->ip, lifc->mask, lifc->net);

			for(l=&nifc->lifc; *l; l=&(*l)->next)
				;
			*l = lifc;
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6*)ifa->ifa_addr;
			lifc = mallocz(sizeof *lifc, 1);
			if(lifc == nil)
				break;
			memmove(lifc->ip, &sin6->sin6_addr, IPaddrlen);

			if(ifa->ifa_netmask){
				struct sockaddr_in6 *nm6;
				nm6 = (struct sockaddr_in6*)ifa->ifa_netmask;
				memmove(lifc->mask, &nm6->sin6_addr, IPaddrlen);
			}
			maskip(lifc->ip, lifc->mask, lifc->net);

			for(l=&nifc->lifc; *l; l=&(*l)->next)
				;
			*l = lifc;
			break;
		}
	}

	freeifaddrs(ifap);
	return list;
}
