/* SPDX-License-Identifier: GPL-2.0 */
#ifndef KB_DUMMY_I2C_EMULATOR_H
#define KB_DUMMY_I2C_EMULATOR_H

#include <linux/errno.h>
#include <linux/i2c.h>

#define DUMMY_I2C_ADDR		0x42
#define DUMMY_I2C_ID_REG	0x00
#define DUMMY_I2C_VALUE_REG	0x01
#define DUMMY_I2C_ID		0xa5

struct dummy_i2c_state {
	u8 value;
};

static bool dummy_i2c_msg_matches(const struct i2c_msg *msg, u16 flags, u16 len)
{
	/* I2C_M_DMA_SAFE describes the buffer, not the wire protocol. */
	return (msg->flags & ~I2C_M_DMA_SAFE) == flags && msg->len == len;
}

static int dummy_i2c_xfer(struct i2c_adapter *adapter,
			  struct i2c_msg *msgs, int num)
{
	struct dummy_i2c_state *state = i2c_get_adapdata(adapter);
	int i;

	for (i = 0; i < num; i++) {
		if (msgs[i].addr != DUMMY_I2C_ADDR)
			return -ENXIO;
	}

	if (num == 1 && dummy_i2c_msg_matches(&msgs[0], 0, 2)) {
		if (msgs[0].buf[0] == DUMMY_I2C_ID_REG)
			return -EACCES;
		if (msgs[0].buf[0] != DUMMY_I2C_VALUE_REG)
			return -EINVAL;
		state->value = msgs[0].buf[1];
		return 1;
	}

	if (num == 2 && dummy_i2c_msg_matches(&msgs[0], 0, 1) &&
	    dummy_i2c_msg_matches(&msgs[1], I2C_M_RD, 1)) {
		switch (msgs[0].buf[0]) {
		case DUMMY_I2C_ID_REG:
			msgs[1].buf[0] = DUMMY_I2C_ID;
			return 2;
		case DUMMY_I2C_VALUE_REG:
			msgs[1].buf[0] = state->value;
			return 2;
		default:
			return -EINVAL;
		}
	}

	return -EOPNOTSUPP;
}

static u32 dummy_i2c_functionality(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C;
}

static const struct i2c_algorithm dummy_i2c_algorithm = {
	.xfer = dummy_i2c_xfer,
	.functionality = dummy_i2c_functionality,
};

#endif
