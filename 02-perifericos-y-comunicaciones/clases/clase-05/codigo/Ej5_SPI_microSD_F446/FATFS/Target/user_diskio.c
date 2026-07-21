/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   Disk I/O driver for microSD using SPI2 and FatFs.
 ******************************************************************************
  */
/* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include "main.h"
//#include "spi.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Comandos SD en modo SPI */
#define CMD0    (0)        /* GO_IDLE_STATE */
#define CMD1    (1)        /* SEND_OP_COND */
#define CMD8    (8)        /* SEND_IF_COND */
#define CMD9    (9)        /* SEND_CSD */
#define CMD12   (12)       /* STOP_TRANSMISSION */
#define CMD16   (16)       /* SET_BLOCKLEN */
#define CMD17   (17)       /* READ_SINGLE_BLOCK */
#define CMD18   (18)       /* READ_MULTIPLE_BLOCK */
#define CMD23   (23)       /* SET_BLOCK_COUNT */
#define CMD24   (24)       /* WRITE_BLOCK */
#define CMD25   (25)       /* WRITE_MULTIPLE_BLOCK */
#define CMD55   (55)       /* APP_CMD */
#define CMD58   (58)       /* READ_OCR */

#define ACMD23  (0x80 + 23)
#define ACMD41  (0x80 + 41)

/* Tipos de tarjeta */
#define CT_MMC      0x01
#define CT_SD1      0x02
#define CT_SD2      0x04
#define CT_SDC      (CT_SD1 | CT_SD2)
#define CT_BLOCK    0x08

/* Private variables ---------------------------------------------------------*/

/* Estado del disco */
static volatile DSTATUS Stat = STA_NOINIT;

/* Tipo de tarjeta detectada */
static BYTE CardType;

/* La instancia SPI2 la genera CubeIDE en spi.c */
extern SPI_HandleTypeDef hspi2;

/* Funciones auxiliares ------------------------------------------------------*/

static void CS_HIGH(void)
{
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
}

static void CS_LOW(void)
{
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
}

static BYTE spi_txrx(BYTE data)
{
  BYTE rx = 0xFF;

  HAL_SPI_TransmitReceive(&hspi2, &data, &rx, 1, HAL_MAX_DELAY);

  return rx;
}

static void spi_send_multi(const BYTE *buff, UINT count)
{
  HAL_SPI_Transmit(&hspi2, (uint8_t*)buff, count, HAL_MAX_DELAY);
}

static void spi_recv_multi(BYTE *buff, UINT count)
{
  BYTE tx = 0xFF;

  while (count--)
  {
    HAL_SPI_TransmitReceive(&hspi2, &tx, buff++, 1, HAL_MAX_DELAY);
  }
}

static int wait_ready(UINT timeout_ms)
{
  BYTE d;
  uint32_t start = HAL_GetTick();

  do
  {
    d = spi_txrx(0xFF);

    if (d == 0xFF)
    {
      return 1;
    }

  } while ((HAL_GetTick() - start) < timeout_ms);

  return 0;
}

static void deselect_card(void)
{
  CS_HIGH();
  spi_txrx(0xFF);
}

static int select_card(void)
{
  CS_LOW();
  spi_txrx(0xFF);

  if (wait_ready(500))
  {
    return 1;
  }

  deselect_card();
  return 0;
}

static int receive_data_block(BYTE *buff, UINT btr)
{
  BYTE token;
  uint32_t start = HAL_GetTick();

  do
  {
    token = spi_txrx(0xFF);

    if (token == 0xFE)
    {
      break;
    }

  } while ((HAL_GetTick() - start) < 200);

  if (token != 0xFE)
  {
    return 0;
  }

  spi_recv_multi(buff, btr);

  /* Descartar CRC */
  spi_txrx(0xFF);
  spi_txrx(0xFF);

  return 1;
}

static int transmit_data_block(const BYTE *buff, BYTE token)
{
  BYTE resp;

  if (!wait_ready(500))
  {
    return 0;
  }

  spi_txrx(token);

  if (token != 0xFD)
  {
    spi_send_multi(buff, 512);

    /* CRC dummy */
    spi_txrx(0xFF);
    spi_txrx(0xFF);

    resp = spi_txrx(0xFF);

    if ((resp & 0x1F) != 0x05)
    {
      return 0;
    }
  }

  return 1;
}

static BYTE send_cmd(BYTE cmd, DWORD arg)
{
  BYTE n;
  BYTE res;

  if (cmd & 0x80)
  {
    cmd &= 0x7F;

    res = send_cmd(CMD55, 0);

    if (res > 1)
    {
      return res;
    }
  }

  deselect_card();

  if (!select_card())
  {
    return 0xFF;
  }

  spi_txrx(0x40 | cmd);
  spi_txrx((BYTE)(arg >> 24));
  spi_txrx((BYTE)(arg >> 16));
  spi_txrx((BYTE)(arg >> 8));
  spi_txrx((BYTE)arg);

  n = 0x01;

  if (cmd == CMD0)
  {
    n = 0x95;
  }

  if (cmd == CMD8)
  {
    n = 0x87;
  }

  spi_txrx(n);

  if (cmd == CMD12)
  {
    spi_txrx(0xFF);
  }

  n = 10;

  do
  {
    res = spi_txrx(0xFF);
  } while ((res & 0x80) && --n);

  return res;
}

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);

#if _USE_WRITE == 1
DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif

#if _USE_IOCTL == 1
DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif

Diskio_drvTypeDef USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,

#if _USE_WRITE
  USER_write,
#endif

#if _USE_IOCTL == 1
  USER_ioctl,
#endif
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize(BYTE pdrv)
{
  /* USER CODE BEGIN INIT */

  BYTE n;
  BYTE cmd;
  BYTE ty;
  BYTE ocr[4];
  uint32_t start;

  (void)pdrv;

  CS_HIGH();
  HAL_Delay(10);

  /*
   * La SD requiere al menos 74 ciclos de clock con CS en alto
   * antes de iniciar el modo SPI.
   */
  for (n = 0; n < 10; n++)
  {
    spi_txrx(0xFF);
  }

  ty = 0;

  if (send_cmd(CMD0, 0) == 1)
  {
    start = HAL_GetTick();

    if (send_cmd(CMD8, 0x1AA) == 1)
    {
      for (n = 0; n < 4; n++)
      {
        ocr[n] = spi_txrx(0xFF);
      }

      if (ocr[2] == 0x01 && ocr[3] == 0xAA)
      {
        while ((HAL_GetTick() - start) < 1000)
        {
          if (send_cmd(ACMD41, 1UL << 30) == 0)
          {
            break;
          }
        }

        if (((HAL_GetTick() - start) < 1000) && (send_cmd(CMD58, 0) == 0))
        {
          for (n = 0; n < 4; n++)
          {
            ocr[n] = spi_txrx(0xFF);
          }

          ty = (ocr[0] & 0x40) ? (CT_SD2 | CT_BLOCK) : CT_SD2;
        }
      }
    }
    else
    {
      if (send_cmd(ACMD41, 0) <= 1)
      {
        ty = CT_SD1;
        cmd = ACMD41;
      }
      else
      {
        ty = CT_MMC;
        cmd = CMD1;
      }

      while ((HAL_GetTick() - start) < 1000)
      {
        if (send_cmd(cmd, 0) == 0)
        {
          break;
        }
      }

      if (!((HAL_GetTick() - start) < 1000) || (send_cmd(CMD16, 512) != 0))
      {
        ty = 0;
      }
    }
  }

  CardType = ty;
  deselect_card();

  if (ty)
  {
    Stat &= ~STA_NOINIT;
  }
  else
  {
    Stat = STA_NOINIT;
  }

  return Stat;

  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status(BYTE pdrv)
{
  /* USER CODE BEGIN STATUS */

  (void)pdrv;

  return Stat;

  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number
  * @param  buff: Data buffer
  * @param  sector: Sector address
  * @param  count: Number of sectors
  * @retval DRESULT: Operation result
  */
DRESULT USER_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
  /* USER CODE BEGIN READ */

  (void)pdrv;

  if (!count)
  {
    return RES_PARERR;
  }

  if (Stat & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  if (!(CardType & CT_BLOCK))
  {
    sector *= 512;
  }

  if (count == 1)
  {
    if ((send_cmd(CMD17, sector) == 0) && receive_data_block(buff, 512))
    {
      count = 0;
    }
  }
  else
  {
    if (send_cmd(CMD18, sector) == 0)
    {
      do
      {
        if (!receive_data_block(buff, 512))
        {
          break;
        }

        buff += 512;

      } while (--count);

      send_cmd(CMD12, 0);
    }
  }

  deselect_card();

  return count ? RES_ERROR : RES_OK;

  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number
  * @param  buff: Data to be written
  * @param  sector: Sector address
  * @param  count: Number of sectors
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
  /* USER CODE BEGIN WRITE */

  (void)pdrv;

  if (!count)
  {
    return RES_PARERR;
  }

  if (Stat & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  if (!(CardType & CT_BLOCK))
  {
    sector *= 512;
  }

  if (count == 1)
  {
    if ((send_cmd(CMD24, sector) == 0) && transmit_data_block(buff, 0xFE))
    {
      count = 0;
    }
  }
  else
  {
    if (CardType & CT_SDC)
    {
      send_cmd(ACMD23, count);
    }

    if (send_cmd(CMD25, sector) == 0)
    {
      do
      {
        if (!transmit_data_block(buff, 0xFC))
        {
          break;
        }

        buff += 512;

      } while (--count);

      if (!transmit_data_block(0, 0xFD))
      {
        count = 1;
      }
    }
  }

  deselect_card();

  return count ? RES_ERROR : RES_OK;

  /* USER CODE END WRITE */
}
#endif

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number
  * @param  cmd: Control code
  * @param  buff: Buffer
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
  /* USER CODE BEGIN IOCTL */

  DRESULT res;
  BYTE n;
  BYTE csd[16];
  DWORD csize;

  (void)pdrv;

  if (Stat & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  res = RES_ERROR;

  switch (cmd)
  {
    case CTRL_SYNC:
      if (select_card())
      {
        res = RES_OK;
      }
      break;

    case GET_SECTOR_COUNT:
      if ((send_cmd(CMD9, 0) == 0) && receive_data_block(csd, 16))
      {
        if ((csd[0] >> 6) == 1)
        {
          csize = csd[9]
                + ((WORD)csd[8] << 8)
                + ((DWORD)(csd[7] & 63) << 16)
                + 1;

          *(DWORD*)buff = csize << 10;
        }
        else
        {
          n = (csd[5] & 15)
            + ((csd[10] & 128) >> 7)
            + ((csd[9] & 3) << 1)
            + 2;

          csize = (csd[8] >> 6)
                + ((WORD)csd[7] << 2)
                + ((WORD)(csd[6] & 3) << 10)
                + 1;

          *(DWORD*)buff = csize << (n - 9);
        }

        res = RES_OK;
      }
      break;

    case GET_SECTOR_SIZE:
      *(WORD*)buff = 512;
      res = RES_OK;
      break;

    case GET_BLOCK_SIZE:
      *(DWORD*)buff = 1;
      res = RES_OK;
      break;

    default:
      res = RES_PARERR;
      break;
  }

  deselect_card();

  return res;

  /* USER CODE END IOCTL */
}
#endif
