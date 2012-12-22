/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 * This code is licensed under the GNU GPLv2 and later.
 */

// Heavily based on milkymist-vgafb.c, copyright terms below.

/*
 *  QEMU model of the Milkymist VGA framebuffer.
 *
 *  Copyright (c) 2010-2012 Michael Walle <michael@walle.cc>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Specification available at:
 *   http://www.milkymist.org/socdoc/vgafb.pdf
 */

#include "sysbus.h"
#include "qemu-common.h"
#include "qdev.h"
#include "console.h"
#include "framebuffer.h"
#include "pixel_ops.h"

#include "cpu-common.h"

#include "bcm2835_common.h"

#define BITS 8
#include "milkymist-vgafb_template.h"
#define BITS 15
#include "milkymist-vgafb_template.h"
#define BITS 16
#include "milkymist-vgafb_template.h"
#define BITS 24
#include "milkymist-vgafb_template.h"
#define BITS 32
#include "milkymist-vgafb_template.h"

typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;

    int pending;
    qemu_irq mbox_irq;
    
    DisplayState *ds;
    int invalidate;
    int enabled;
    
    uint32_t xres, yres;
    uint32_t xres_virtual, yres_virtual;
    uint32_t xoffset, yoffset;
    uint32_t bpp;
    uint32_t base, pitch, size;
} bcm2835_fb_state;

static void fb_invalidate_display(void *opaque)
{
    bcm2835_fb_state *s = (bcm2835_fb_state *)opaque;
    s->invalidate = 1;
}
static void fb_update_display(void *opaque)
{
    bcm2835_fb_state *s = (bcm2835_fb_state *)opaque;
    int first = 0;
    int last = 0;
    drawfn fn;

    int src_width = 0;
    int dest_width = 0;
    
    if (!s->enabled)
        return;
    
    // Assuming source is 16bpp for now
    src_width = s->xres * 2;
    
    dest_width = s->xres;
    switch (ds_get_bits_per_pixel(s->ds)) {
    case 0:
        return;
    case 8:
        fn = draw_line_8;
        break;
    case 15:
        fn = draw_line_15;
        dest_width *= 2;
        break;
    case 16:
        fn = draw_line_16;
        dest_width *= 2;
        break;
    case 24:
        fn = draw_line_24;
        dest_width *= 3;
        break;
    case 32:
        fn = draw_line_32;
        dest_width *= 4;
        break;
    default:
        hw_error("milkymist_vgafb: bad color depth\n");
        break;
    }

    framebuffer_update_display(s->ds, sysbus_address_space(&s->busdev),
        s->base,
        s->xres,
        s->yres,
        src_width,
        dest_width,
        0,
        s->invalidate,
        fn,
        NULL,
        &first, &last);
    if (first >= 0) {
        dpy_gfx_update(s->ds, 0, first, s->xres, last - first + 1);
    }

    s->invalidate = 0;
}



static void bcm2835_fb_mbox_push(bcm2835_fb_state *s, uint32_t value) 
{
    value &= ~0xf;
    
    s->xres = ldl_phys(value);
    s->yres = ldl_phys(value + 4);
    s->xres_virtual = ldl_phys(value + 8);
    s->yres_virtual = ldl_phys(value + 12);
    
    s->bpp = ldl_phys(value + 20);
    s->xoffset = ldl_phys(value + 24);
    s->yoffset = ldl_phys(value + 28);
    
    s->base = bcm2835_vcram_base | (value & 0xc0000000);

    assert(s->bpp == 16);

    // TODO - Manage properly virtual resolution
    /*if (s->bpp == 16) {
        s->pitch = ((s->xres_virtual + 1) & ~1) * 2;
    }
    s->size = s->yres_virtual * s->pitch; 
    */
    s->pitch = s->xres * 2;
    s->size = s->yres * s->pitch;
    
    stl_phys(value + 16, s->pitch);
    stl_phys(value + 32, s->base);
    stl_phys(value + 36, s->size);
    
    qemu_console_resize(s->ds, s->xres, s->yres);
    s->enabled = 1;
    s->invalidate = 1;    
}

static uint64_t bcm2835_fb_read(void *opaque, hwaddr offset,
    unsigned size)
{
    bcm2835_fb_state *s = (bcm2835_fb_state *)opaque;
    uint32_t res = 0;
    
    switch(offset) {
    case 0:
        res = MBOX_CHAN_FB;
        s->pending = 0;
        qemu_set_irq(s->mbox_irq, 0);
        break;
    case 4:
        res = s->pending;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "bcm2835_fb_read: Bad offset %x\n", (int)offset);
        return 0;        
    }
    return res;
}
static void bcm2835_fb_write(void *opaque, hwaddr offset,
    uint64_t value, unsigned size)
{
    bcm2835_fb_state *s = (bcm2835_fb_state *)opaque;
    switch(offset) {
    case 0:
        if (!s->pending) {
            s->pending = 1;
            bcm2835_fb_mbox_push(s, value);
            qemu_set_irq(s->mbox_irq, 1);
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "bcm2835_fb_write: Bad offset %x\n", (int)offset);
        return;        
    }    
}


static const MemoryRegionOps bcm2835_fb_ops = {
    .read = bcm2835_fb_read,
    .write = bcm2835_fb_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_bcm2835_fb = {
    .name = "bcm2835_fb",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static int bcm2835_fb_init(SysBusDevice *dev)
{
    bcm2835_fb_state *s = FROM_SYSBUS(bcm2835_fb_state, dev);
    
    s->pending = 0;
    
    s->invalidate = 0;
    s->enabled = 0;
        
    sysbus_init_irq(dev, &s->mbox_irq);
    
    s->ds = graphic_console_init(fb_update_display,
        fb_invalidate_display,
        NULL, NULL, s);

    memory_region_init_io(&s->iomem, &bcm2835_fb_ops, s, "bcm2835_fb", 0x10);
    sysbus_init_mmio(dev, &s->iomem);
    vmstate_register(&dev->qdev, -1, &vmstate_bcm2835_fb, s);

    return 0;
}

static void bcm2835_fb_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = bcm2835_fb_init;
}

static TypeInfo bcm2835_fb_info = {
    .name          = "bcm2835_fb",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(bcm2835_fb_state),
    .class_init    = bcm2835_fb_class_init,
};

static void bcm2835_fb_register_types(void)
{
    type_register_static(&bcm2835_fb_info);
}

type_init(bcm2835_fb_register_types)
