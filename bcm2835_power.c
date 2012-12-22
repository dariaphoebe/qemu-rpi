/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 * This code is licensed under the GNU GPLv2 and later.
 */

#include "sysbus.h"
#include "qemu-common.h"
#include "qdev.h"

#include "bcm2835_common.h"

typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;
    int pending;
    qemu_irq mbox_irq;
} bcm2835_power_state;

static uint64_t bcm2835_power_read(void *opaque, hwaddr offset,
    unsigned size)
{
    bcm2835_power_state *s = (bcm2835_power_state *)opaque;
    uint32_t res = 0;
    
    switch(offset) {
    case 0:
        res = MBOX_CHAN_POWER;
        s->pending = 0;
        qemu_set_irq(s->mbox_irq, 0);
        break;
    case 4:
        res = s->pending;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "bcm2835_power_read: Bad offset %x\n", (int)offset);
        return 0;        
    }
    return res;
}
static void bcm2835_power_write(void *opaque, hwaddr offset,
    uint64_t value, unsigned size)
{
    bcm2835_power_state *s = (bcm2835_power_state *)opaque;
    switch(offset) {
    case 0:
        s->pending = 1;
        qemu_set_irq(s->mbox_irq, 1);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "bcm2835_power_write: Bad offset %x\n", (int)offset);
        return; 
    }
    
}


static const MemoryRegionOps bcm2835_power_ops = {
    .read = bcm2835_power_read,
    .write = bcm2835_power_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


static const VMStateDescription vmstate_bcm2835_power = {
    .name = "bcm2835_power",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static int bcm2835_power_init(SysBusDevice *dev)
{
    bcm2835_power_state *s = FROM_SYSBUS(bcm2835_power_state, dev);
    
    s->pending = 0;
    
    sysbus_init_irq(dev, &s->mbox_irq);
    memory_region_init_io(&s->iomem, &bcm2835_power_ops, s, 
        "bcm2835_power", 0x10);
    sysbus_init_mmio(dev, &s->iomem);
    vmstate_register(&dev->qdev, -1, &vmstate_bcm2835_power, s);

    return 0;
}

static void bcm2835_power_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);
    // DeviceClass *k = DEVICE_CLASS(klass);

    sdc->init = bcm2835_power_init;
}

static TypeInfo bcm2835_power_info = {
    .name          = "bcm2835_power",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(bcm2835_power_state),
    .class_init    = bcm2835_power_class_init,
};

static void bcm2835_power_register_types(void)
{
    type_register_static(&bcm2835_power_info);
}

type_init(bcm2835_power_register_types)
