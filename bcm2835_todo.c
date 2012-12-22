/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 * This code is licensed under the GNU GPLv2 and later.
 */

#include "sysbus.h"
#include "qemu-common.h"
#include "qdev.h"

// #define LOG_UNMAPPED_ACCESS

typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;
} bcm2835_todo_state;

static uint64_t bcm2835_todo_read(void *opaque, hwaddr offset,
    unsigned size)
{
#ifdef LOG_UNMAPPED_ACCESS
    printf("bcm2835: unmapped read(%x)\n", (int)offset);
#endif
    return 0;
}

static void bcm2835_todo_write(void *opaque, hwaddr offset,
    uint64_t value, unsigned size)
{
#ifdef LOG_UNMAPPED_ACCESS
    printf("bcm2835: unmapped write(%x) %llx\n", (int)offset, value);
#endif
}

static const MemoryRegionOps bcm2835_todo_ops = {
    .read = bcm2835_todo_read,
    .write = bcm2835_todo_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_bcm2835_todo = {
    .name = "bcm2835_todo",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static int bcm2835_todo_init(SysBusDevice *dev)
{
    bcm2835_todo_state *s = FROM_SYSBUS(bcm2835_todo_state, dev);
    
    memory_region_init_io(&s->iomem, &bcm2835_todo_ops, s, 
        "bcm2835_todo", 0x1000000);
    sysbus_init_mmio(dev, &s->iomem);
    
    vmstate_register(&dev->qdev, -1, &vmstate_bcm2835_todo, s);

    return 0;
}

static void bcm2835_todo_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);
 
    sdc->init = bcm2835_todo_init;
}

static TypeInfo bcm2835_todo_info = {
    .name          = "bcm2835_todo",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(bcm2835_todo_state),
    .class_init    = bcm2835_todo_class_init,
};

static void bcm2835_todo_register_types(void)
{
    type_register_static(&bcm2835_todo_info);
}

type_init(bcm2835_todo_register_types)
