#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CONFIG_FILE "devconfig"
#define BUF_SIZE 1024
#define CMP_SIZE 3
#define DEV_NAME_SIZE 32
#define MAC_SIZE 6
#define MAX_VLAN_SIZE 4096

#define TRUNK "trunk"
#define ACCESS "access"

#define MOD_ACCESS 0
#define MOD_TRUNK 1

char default_dev[DEV_NAME_SIZE] = {"eth1"};
char vlanMac[MAC_SIZE] = {0x00, 0x03, 0x0f, 0x00, 0x00, 0x01};
uint32_t hostaddr = 0;
char vlanMod[MAX_VLAN_SIZE] = {0};

struct vlan_device_entry
{
    struct vlan_device_entry *next;
    int vid;
    char *dev;
};

struct vlan_device_entry *vlandev = NULL;

struct port_device_entry
{
    struct port_device_entry *next;
    int portid;
    char *dev;
};

struct port_device_entry *portdev = NULL;

struct config_entry
{
    struct config_entry *next;
    char *name;
    int (* analyze) (char *str);
};

struct config_entry *devcf = NULL;

void addConfigEntry(char *name, int (* analyze) (char *str))
{
    struct config_entry *p = NULL;

    p = malloc(sizeof(struct config_entry));
    if (p == NULL)
    {
        perror("malloc config_entry error");
        return;
    }

    p->name = malloc(strlen(name));
    if (p->name == NULL)
    {
        perror("malloc name error");
        free(p);
        return;
    }
    
    memcpy(p->name, name, strlen(name)+1);
    p->analyze = analyze;
    p->next = NULL;

    if (devcf == NULL)
    {
        devcf = p;
    }
    else
    {
        struct config_entry *tmp = devcf;
        while(tmp->next != NULL) {tmp = tmp->next;};
        tmp->next = p;
    }

    return;
}

void addVlanDev(int vid, char *devname)
{
    struct vlan_device_entry *p = NULL;

    p = malloc(sizeof(struct vlan_device_entry));
    if (p == NULL)
    {
        perror("malloc vlan_device_entry error");
        return;
    }

    p->dev = malloc(strlen(devname));
    if (p->dev == NULL)
    {
        perror("malloc devname error");
        free(p);
        return;
    }
    
    memcpy(p->dev, devname, strlen(devname)+1);
    p->vid = vid;
    p->next = NULL;

    if (vlandev == NULL)
    {
        vlandev = p;
    }
    else
    {
        struct vlan_device_entry *tmp = vlandev;
        while(tmp->next != NULL) {tmp = tmp->next;};
        tmp->next = p;
    }

    return;
}

char *getVlanDev(int vid)
{
    struct vlan_device_entry *tmp = vlandev;

    if (tmp == NULL)
    {
        return default_dev;
    }
    
    while (tmp != NULL)
    {
        if (tmp->vid == vid)
        {
            return tmp->dev;
        }

        tmp = tmp->next;
    }

    return default_dev;
}

void addPortDev(int portid, char *devname)
{
    struct port_device_entry *p = NULL;

    p = malloc(sizeof(struct port_device_entry));
    if (p == NULL)
    {
        perror("malloc port_device_entry error");
        return;
    }

    p->dev = malloc(strlen(devname));
    if (p->dev == NULL)
    {
        perror("malloc devname error");
        free(p);
        return;
    }
    
    memcpy(p->dev, devname, strlen(devname)+1);
    p->portid = portid;
    p->next = NULL;

    if (portdev == NULL)
    {
        portdev = p;
    }
    else
    {
        struct port_device_entry *tmp = portdev;
        while(tmp->next != NULL) {tmp = tmp->next;};
        tmp->next = p;
    }

    return;
}

char *getPortDev(int portid)
{
    struct port_device_entry *tmp = portdev;

    if (tmp == NULL)
    {
        return default_dev;
    }
    
    while (tmp != NULL)
    {
        if (tmp->portid == portid)
        {
            return tmp->dev;
        }

        tmp = tmp->next;
    }

    return default_dev;
}

int xtoi(char *str)
{
    int ret = 0;
    int i = 0;
    while (str[i] != '\0')
    {
        if (str[i]>='a' && str[i]<'f')
        {
            ret += (str[i] - 'a' + 10);
        }
        else
        {
            ret += str[i] - '0';
        }

        ret *= 16;
        i++;
    }

    return ret/16;
}

int analyze_mac (char *str)
{
    char *p = strtok(str, " ");
    char *macstr = strtok(NULL, " ");
    int i = 0;
    char *macele = strtok(macstr, ":");
    printf("%s: %s\n", __func__, p);
    for (i=0; i<MAC_SIZE; i++)
    {
        vlanMac[i] = xtoi(macele);
        macele = strtok(NULL, ":");
    }
    
    return 0;
}

int analyze_port (char *str)
{
    char *port = NULL; 
    char *dev  = NULL;
    int portid = 0;

    port= strtok(str, " ");
    dev = strtok(NULL, " ");
    portid = atoi(strtok(port, "port"));

    printf("%s: %d-%s \n", __func__, portid, dev);

    addPortDev(portid, dev);

    return 0;
}

int analyze_vlan (char *str)
{
    char *vlan = NULL; 
    char *dev = NULL;
    char *mod = NULL;
    int vid = 0;
    int devmod = MOD_ACCESS;

    vlan = strtok(str, " ");
    dev = strtok(NULL, " ");
    mod = strtok(NULL, " ");
    vid = atoi(strtok(vlan, "vlan"));
    if ((mod != NULL) && (!strcmp(mod, TRUNK)))
    {
        devmod = MOD_TRUNK;
    }
    printf("%s: %d-%s mod:%s-%d\n", __func__, vid, dev, mod, devmod);

    vlanMod[vid] = devmod;
    addVlanDev(vid, dev);

    return 0;
}

int analyze_defdev (char *str)
{
    char *p = strtok(str, " ");
    p = strtok(NULL, " ");
    printf("%s: %s\n", __func__, p);

    memcpy(default_dev, p, strlen(str)+1);

    return 0;
}

int analyze_hostip (char *str)
{
    char *p = strtok(str, " ");
    p = strtok(NULL, " ");
    printf("%s: %s\n", __func__, p);
    
    hostaddr = inet_addr(p);
    
    return 0;
}

int main()
{
    FILE *f = NULL;
    char buf[BUF_SIZE] = {'\0'};
    char *ret = 0;

    addConfigEntry("mac", analyze_mac);
    addConfigEntry("vlan", analyze_vlan);
    addConfigEntry("defdev", analyze_defdev);
    addConfigEntry("hostip", analyze_hostip);
    addConfigEntry("port", analyze_port);

    f = fopen(CONFIG_FILE, "r+");
    if (f == NULL)
    {
        perror("open file error");
        return -1;
    }

    while ((ret = fgets(buf, BUF_SIZE, f)) != NULL)
    {
        struct config_entry *tmp = devcf;

        if (ret[strlen(ret)-1] == '\n')
        {
            ret[strlen(ret)-1] = '\0';
        }

        while (tmp != NULL)
        {
            if (!strncmp(tmp->name, buf, CMP_SIZE))
            {
                tmp->analyze(buf);
                break;
            }
            tmp = tmp->next;
        }

        memset(buf, 0, BUF_SIZE);
    }

    printf("\n");
    printf("default dev = %s\n", default_dev);
    printf("vlan2 eth = %s\n", getVlanDev(2));
    printf("vlan3 eth = %s\n", getVlanDev(3));
    printf("local vlan mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
            vlanMac[0], vlanMac[1], vlanMac[2],
            vlanMac[3], vlanMac[4], vlanMac[5]);
    printf("host address = %08x\n", hostaddr);
    printf("port1 eth = %s\n", getPortDev(1));
    printf("port1 eth = %s\n", getPortDev(2));
    
    return 0;
}
