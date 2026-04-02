
```c
/**
 * struct component_ops - callbacks for component drivers
 *
 * Components are registered with component_add() and unregistered with
 * component_del().
 */
struct component_ops {
	/**
	 * @bind:
	 *
	 * Called through component_bind_all() when the aggregate driver is
	 * ready to bind the overall driver.
	 */
	int (*bind)(struct device *comp, struct device *master,
		    void *master_data);
	/**
	 * @unbind:
	 *
	 * Called through component_unbind_all() when the aggregate driver is
	 * ready to bind the overall driver, or when component_bind_all() fails
	 * part-ways through and needs to unbind some already bound components.
	 */
	void (*unbind)(struct device *comp, struct device *master,
		       void *master_data);
};
```

### Example

```c
static const struct component_ops sun4i_tcon_ops = {
	.bind	= sun4i_tcon_bind,
	.unbind	= sun4i_tcon_unbind,
};

static int sun4i_tcon_probe(struct platform_device *pdev)
{
	...
	
	return component_add(&pdev->dev, &sun4i_tcon_ops);
}

static void sun4i_tcon_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &sun4i_tcon_ops);
}
```