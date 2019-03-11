/*
 *copyright (c) 2017 Nuvoton Technology Corp.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <dm.h>
#include <sdhci.h>
#include <clk.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/arch/clock.h>

#define NPCMX50_SD          0
#define NPCMX50_EMMC        1

#ifdef CONFIG_DM_MMC
struct npcmx50_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

DECLARE_GLOBAL_DATA_PTR;
#endif

/* TODO: move this to GCR driver. */
static int gcr_mux_sd(int index)
{
	struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();

	if (index == NPCMX50_SD) {
		writel(readl(&gcr->mfsel3) | (1 << MFSEL3_SD1SEL),
				&gcr->mfsel3);
	} else if (index == NPCMX50_EMMC) {
		unsigned int id;
		id = readl(&gcr->pdid) & 0xFFFFFF;
		if (id == POLEG_Z1) {
			writel(readl(&gcr->mfsel3) | (1 << MFSEL3_MMCSEL) |
				(1 << MFSEL3_SMB13SEL) | (1 << MFSEL3_MMC8SEL),
				&gcr->mfsel3);
		} else {
			printf("board NOT support\n");
			return -1;
		}
	}

	return 0;
}

#ifdef CONFIG_DM_MMC
static int npcmx50_sdhci_probe(struct udevice *dev)
{
	struct npcmx50_sdhci_plat *plat = dev_get_platdata(dev);
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct sdhci_host *host = dev_get_priv(dev);
	int ret;

	ret = gcr_mux_sd(host->index);
	if (ret)
		return ret;

	host->quirks = SDHCI_QUIRK_NO_HISPD_BIT | SDHCI_QUIRK_BROKEN_VOLTAGE |
			SDHCI_QUIRK_32BIT_DMA_ADDR |
			SDHCI_QUIRK_WAIT_SEND_CMD | SDHCI_QUIRK_USE_WIDE8;
	host->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;

	host->version = sdhci_readw(host, SDHCI_HOST_VERSION);
	if (host->bus_width == 4)
		host->host_caps |= MMC_MODE_4BIT;

	if (host->bus_width == 8)
		host->host_caps |= MMC_MODE_8BIT;

	ret = sdhci_setup_cfg(&plat->cfg, host, host->clock, 400000);
	if (ret)
		return ret;

	host->mmc = &plat->mmc;
	host->mmc->priv = host;
	host->mmc->dev = dev;
	upriv->mmc = host->mmc;

	host->clock = 0;

	return sdhci_probe(dev);
}

static int npcmx50_ofdata_to_platdata(struct udevice *dev)
{
	struct sdhci_host *host = dev_get_priv(dev);

	host->name = strdup(dev->name);
	host->ioaddr = (void *)dev_read_addr(dev);
	host->bus_width = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
			"bus-width", 4);
	host->index = fdtdec_get_uint(gd->fdt_blob, dev_of_offset(dev), "index", 0);
	host->clock = fdtdec_get_uint(gd->fdt_blob, dev_of_offset(dev),
			"clock-frequency", 400000);

	if (host->ioaddr == (void *)FDT_ADDR_T_NONE)
		return -EINVAL;

	return 0;
}

static int npcmx50_sdhci_bind(struct udevice *dev)
{
	struct npcmx50_sdhci_plat *plat = dev_get_platdata(dev);
	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static int npcmx50_sdhci_remove(struct udevice *dev)
{
	return 0;
}

static const struct udevice_id npcmx50_mmc_ids[] = {
	{ .compatible = "nuvoton,npcmx50-sdhci-SD" },
	{ .compatible = "nuvoton,npcmx50-sdhci-eMMC"},
	{ }
};

U_BOOT_DRIVER(npcmx50_sdc_drv) = {
	.name           = "npcmx50_sdhci",
	.id             = UCLASS_MMC,
	.of_match       = npcmx50_mmc_ids,
	.ofdata_to_platdata = npcmx50_ofdata_to_platdata,
	.ops            = &sdhci_ops,
	.bind           = npcmx50_sdhci_bind,
	.probe          = npcmx50_sdhci_probe,
	.remove         = npcmx50_sdhci_remove,
	.priv_auto_alloc_size = sizeof(struct sdhci_host),
	.platdata_auto_alloc_size = sizeof(struct npcmx50_sdhci_plat),
};
#endif