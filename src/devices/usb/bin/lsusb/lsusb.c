// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <endian.h>
#include <fcntl.h>
#include <fuchsia/hardware/usb/device/c/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/fdio.h>
#include <lib/fdio/io.h>
#include <lib/syslog/global.h>
#include <lib/syslog/logger.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zircon/assert.h>
#include <zircon/hw/usb.h>
#include <zircon/hw/usb/hid.h>
#include <zircon/syscalls.h>
#include <zircon/types.h>

#include <pretty/hexdump.h>

#define DEV_USB "/dev/class/usb-device"

#define EN_US 0x0409

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static void get_string_desc(zx_handle_t svc, uint8_t desc_id, char* buffer, size_t buffer_size) {
  if (desc_id) {
    // Default to asking for US english.
    zx_status_t status;
    size_t actual;
    uint16_t actual_lang_id;
    zx_status_t res = fuchsia_hardware_usb_device_DeviceGetStringDescriptor(
        svc, desc_id, EN_US, &status, buffer, buffer_size, &actual, &actual_lang_id);
    if (res == ZX_OK)
      res = status;
    if (res == ZX_OK) {
      if (actual < buffer_size) {
        buffer[actual] = 0;
      }
    } else {
      snprintf(buffer, buffer_size, "<err %d>", res);
    }
  } else {
    snprintf(buffer, buffer_size, "<none>");
  }
}

static const char* usb_speeds[] = {
    "<unknown>", "FULL", "LOW", "HIGH", "SUPER",
};

static int do_list_device(zx_handle_t svc, int configuration, bool verbose, const char* devname,
                          int depth, int max_depth) {
  usb_device_descriptor_info_t device_desc;
  char manufacturer[fuchsia_hardware_usb_device_MAX_STRING_DESC_SIZE];
  char product[fuchsia_hardware_usb_device_MAX_STRING_DESC_SIZE];
  ssize_t ret = 0;

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  ret = fuchsia_hardware_usb_device_DeviceGetDeviceDescriptor(svc, (uint8_t*)&device_desc);

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  if (ret != ZX_OK) {
    printf("DeviceGetDeviceDescriptor failed for %s/%s\n", DEV_USB, devname);
    return ret;
  }

  uint32_t speed;

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  ret = fuchsia_hardware_usb_device_DeviceGetDeviceSpeed(svc, &speed);

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  if (ret != ZX_OK || speed >= countof(usb_speeds)) {
    printf("DeviceGetDeviceSpeed failed for %s/%s\n", DEV_USB, devname);
    return ret;
  }

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  get_string_desc(svc, device_desc.i_manufacturer, manufacturer, sizeof(manufacturer));

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  get_string_desc(svc, device_desc.i_product, product, sizeof(product));

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  int left_pad = depth * 4;
  int right_pad = (max_depth - depth) * 4;
  printf("%*s%3s  %*s%04X:%04X  %-5s  %s %s\n", left_pad, "", devname, right_pad, "",
         le16toh(device_desc.id_vendor), le16toh(device_desc.id_product), usb_speeds[speed],
         manufacturer, product);

  if (verbose) {
    char string_buf[fuchsia_hardware_usb_device_MAX_STRING_DESC_SIZE];

    // print device descriptor
    printf("Device Descriptor:\n");
    printf("  bLength                         %d\n", device_desc.b_length);
    printf("  bDescriptorType                 %d\n", device_desc.b_descriptor_type);
    printf("  bcdUSB                          %x.%x\n", le16toh(device_desc.bcd_usb) >> 8,
           le16toh(device_desc.bcd_usb) & 0xFF);
    printf("  bDeviceClass                    %d\n", device_desc.b_device_class);
    printf("  bDeviceSubClass                 %d\n", device_desc.b_device_sub_class);
    printf("  bDeviceProtocol                 %d\n", device_desc.b_device_protocol);
    printf("  bMaxPacketSize0                 %d\n", device_desc.b_max_packet_size0);
    printf("  idVendor                        0x%04X\n", le16toh(device_desc.id_vendor));
    printf("  idProduct                       0x%04X\n", le16toh(device_desc.id_product));
    printf("  bcdDevice                       %x.%x\n", le16toh(device_desc.bcd_device) >> 8,
           le16toh(device_desc.bcd_device) & 0xFF);
    printf("  iManufacturer                   %d %s\n", device_desc.i_manufacturer, manufacturer);
    printf("  iProduct                        %d %s\n", device_desc.i_product, product);
    get_string_desc(svc, device_desc.i_serial_number, string_buf, sizeof(string_buf));
    printf("  iSerialNumber                   %d %s\n", device_desc.i_serial_number, string_buf);
    printf("  bNumConfigurations              %d\n", device_desc.b_num_configurations);

    if (configuration == -1) {
      uint8_t config;
      FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
      ret = fuchsia_hardware_usb_device_DeviceGetConfiguration(svc, &config);

      FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
      if (ret != ZX_OK)
        return ret;
      configuration = config;
    }

    // Read header of configuration descriptor to get total length.
    zx_status_t status;
    uint16_t desc_size;

    FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
    ret = fuchsia_hardware_usb_device_DeviceGetConfigurationDescriptorSize(svc, configuration,
                                                                           &status, &desc_size);

    FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
    if (ret == ZX_OK)
      ret = status;
    if (ret != ZX_OK) {
      printf("GetConfigurationDescriptor failed for %s/%s\n", DEV_USB, devname);
      return ret;
    }

    uint8_t* desc = malloc(desc_size);
    if (!desc) {
      ret = -1;
      return ret;
    }

    size_t actual;
    FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
    ret = fuchsia_hardware_usb_device_DeviceGetConfigurationDescriptor(svc, configuration, &status,
                                                                       desc, desc_size, &actual);
    FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
    if (ret == ZX_OK)
      ret = status;
    if (ret != ZX_OK || actual != desc_size) {
      printf("GetConfigurationDescriptor failed for %s/%s\n", DEV_USB, devname);
      goto free_out;
    }

    usb_descriptor_header_t* desc_end = (usb_descriptor_header_t*)(desc + desc_size);

    // print configuration descriptor
    usb_configuration_descriptor_t* config_desc = (usb_configuration_descriptor_t*)desc;
    printf("  Configuration Descriptor:\n");
    printf("    bLength                       %d\n", config_desc->b_length);
    printf("    bDescriptorType               %d\n", config_desc->b_descriptor_type);
    printf("    wTotalLength                  %d\n", le16toh(config_desc->w_total_length));
    printf("    bNumInterfaces                %d\n", config_desc->b_num_interfaces);
    printf("    bConfigurationValue           %d\n", config_desc->b_configuration_value);
    get_string_desc(svc, config_desc->i_configuration, string_buf, sizeof(string_buf));
    printf("    iConfiguration                %d %s\n", config_desc->i_configuration, string_buf);
    printf("    bmAttributes                  0x%02X\n", config_desc->bm_attributes);
    printf("    bMaxPower                     %d\n", config_desc->b_max_power);

    // print remaining descriptors
    usb_descriptor_header_t* header =
        (usb_descriptor_header_t*)(desc + sizeof(usb_configuration_descriptor_t));
    while (header < desc_end) {
      if (header->b_length == 0) {
        printf("zero length header, bailing\n");
        break;
      }
      if (header->b_descriptor_type == USB_DT_INTERFACE) {
        usb_interface_info_descriptor_t* desc = (usb_interface_info_descriptor_t*)header;
        printf("    Interface Descriptor:\n");
        printf("      bLength                     %d\n", desc->b_length);
        printf("      bDescriptorType             %d\n", desc->b_descriptor_type);
        printf("      bInterfaceNumber            %d\n", desc->b_interface_number);
        printf("      bAlternateDetting           %d\n", desc->b_alternate_setting);
        printf("      bNumEndpoints               %d\n", desc->b_num_endpoints);
        printf("      bInterfaceClass             %d\n", desc->b_interface_class);
        printf("      bInterfaceSubClass          %d\n", desc->b_interface_sub_class);
        printf("      bInterfaceProtocol          %d\n", desc->b_interface_protocol);
        get_string_desc(svc, desc->i_interface, string_buf, sizeof(string_buf));
        printf("      i_interface                  %d %s\n", desc->i_interface, string_buf);
      } else if (header->b_descriptor_type == USB_DT_ENDPOINT) {
        usb_endpoint_info_descriptor_t* desc = (usb_endpoint_info_descriptor_t*)header;
        printf("      Endpoint Descriptor:\n");
        printf("        bLength                   %d\n", desc->b_length);
        printf("        bDescriptorType           %d\n", desc->b_descriptor_type);
        printf("        bEndpointAddress          0x%02X\n", desc->b_endpoint_address);
        printf("        bmAttributes              0x%02X\n", desc->bm_attributes);
        printf("        wMaxPacketSize            %d\n", le16toh(desc->w_max_packet_size));
        printf("        bInterval                 %d\n", desc->b_interval);
      } else if (header->b_descriptor_type == USB_DT_HID) {
        usb_hid_descriptor_t* desc = (usb_hid_descriptor_t*)header;
        printf("      HID Descriptor:\n");
        printf("        bLength                   %d\n", desc->bLength);
        printf("        bDescriptorType           %d\n", desc->bDescriptorType);
        printf("        bcdHID                    %x.%x\n", le16toh(desc->bcdHID) >> 8,
               le16toh(desc->bcdHID) & 0xFF);
        printf("        bCountryCode              %d\n", desc->bCountryCode);
        printf("        bNumDescriptors           %d\n", desc->bNumDescriptors);
        for (int i = 0; i < desc->bNumDescriptors; i++) {
          usb_hid_descriptor_entry_t* entry = &desc->descriptors[i];
          printf("          bDescriptorType         %d\n", entry->bDescriptorType);
          printf("          wDescriptorLength       %d\n", le16toh(entry->wDescriptorLength));
        }
      } else if (header->b_descriptor_type == USB_DT_SS_EP_COMPANION) {
        usb_ss_ep_comp_descriptor_info_t* desc = (usb_ss_ep_comp_descriptor_info_t*)header;
        printf("        SuperSpeed Endpoint Companion Descriptor:\n");
        printf("          bLength                 %d\n", desc->b_length);
        printf("          bDescriptorType         %d\n", desc->b_descriptor_type);
        printf("          bMaxBurst               0x%02X\n", desc->b_max_burst);
        printf("          bmAttributes            0x%02X\n", desc->bm_attributes);
        printf("          wBytesPerInterval       %d\n", le16toh(desc->w_bytes_per_interval));
      } else if (header->b_descriptor_type == USB_DT_SS_ISOCH_EP_COMPANION) {
        usb_ss_isoch_ep_comp_descriptor_t* desc = (usb_ss_isoch_ep_comp_descriptor_t*)header;
        printf("        SuperSpeed Isochronous Endpoint Companion Descriptor:\n");
        printf("          bLength                 %d\n", desc->b_length);
        printf("          bDescriptorType         %d\n", desc->b_descriptor_type);
        printf("          wReserved               %d\n", le16toh(desc->w_reserved));
        printf("          dwBytesPerInterval      %d\n", le32toh(desc->dw_bytes_per_interval));
      } else if (header->b_descriptor_type == USB_DT_INTERFACE_ASSOCIATION) {
        usb_interface_assoc_descriptor_t* desc = (usb_interface_assoc_descriptor_t*)header;
        printf("    Interface Association Descriptor:\n");
        printf("      bLength                     %d\n", desc->b_length);
        printf("      bDescriptorType             %d\n", desc->b_descriptor_type);
        printf("      bFirstInterface             %d\n", desc->b_first_interface);
        printf("      bInterfaceCount             %d\n", desc->b_interface_count);
        printf("      bFunctionClass              %d\n", desc->b_function_class);
        printf("      bFunctionSubClass           %d\n", desc->b_function_sub_class);
        printf("      bFunctionProtocol           %d\n", desc->b_function_protocol);
        printf("      iFunction                   %d\n", desc->i_function);
      } else {
        // FIXME - support other descriptor types
        printf("      Unknown Descriptor:\n");
        printf("        bLength                   %d\n", header->b_length);
        printf("        bDescriptorType           %d\n", header->b_descriptor_type);
        hexdump8_ex(header, header->b_length, 0);
      }

      header = (usb_descriptor_header_t*)((uint8_t*)header + header->b_length);
    }

  free_out:
    free(desc);
  }

  return 0;
}

static int list_device(const char* device_id, int configuration, bool verbose) {
  char devname[128];

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  snprintf(devname, sizeof(devname), "%s/%s", DEV_USB, device_id);
  int fd = open(devname, O_RDONLY);

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  if (fd < 0) {
    printf("Error opening %s\n", devname);
    return fd;
  }

  zx_handle_t svc;

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  zx_status_t status = fdio_get_service_handle(fd, &svc);

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  if (status != ZX_OK) {
    close(fd);
    return status;
  }

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  int ret = do_list_device(svc, configuration, verbose, device_id, 0, 0);

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  zx_handle_close(svc);
  close(fd);

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  return ret;
}

static int list_devices(bool verbose) {
  struct dirent* de;
  DIR* dir = opendir(DEV_USB);

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  if (!dir) {
    printf("Error opening %s\n", DEV_USB);
    return -1;
  }

  while ((de = readdir(dir)) != NULL) {
    if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
      list_device(de->d_name, -1, verbose);
    }
  }

  closedir(dir);
  return 0;
}
struct device_node {
  int fd;
  zx_handle_t svc;
  char devname[4];
  uint32_t device_id;
  uint32_t hub_id;
  struct device_node* next;
  int depth;  // depth in tree, or -1 if not computed yet
};

static int get_node_depth(struct device_node* node, struct device_node* devices) {
  if (node->depth >= 0)
    return node->depth;
  if (node->hub_id == 0)
    return 0;

  struct device_node* test_node = devices;
  while (test_node) {
    if (node->hub_id == test_node->device_id) {
      return get_node_depth(test_node, devices) + 1;
    }
    test_node = test_node->next;
  }
  // shouldn't get here
  return -1;
}

static void do_list_tree(struct device_node* devices, uint64_t hub_id, int max_depth) {
  struct device_node* node = devices;
  while (node) {
    if (node->hub_id == hub_id) {
      do_list_device(node->svc, -1, false, node->devname, node->depth, max_depth);
      do_list_tree(devices, node->device_id, max_depth);
    }
    node = node->next;
  }
}

static int list_tree(void) {
  struct dirent* de;
  DIR* dir = opendir(DEV_USB);
  if (!dir) {
    printf("Error opening %s\n", DEV_USB);
    return -1;
  }

  struct device_node* devices = NULL;
  struct device_node* tail = NULL;

  while ((de = readdir(dir)) != NULL) {
    char devname[30];

    snprintf(devname, sizeof(devname), "%s/%s", DEV_USB, de->d_name);
    int fd = open(devname, O_RDONLY);
    if (fd < 0) {
      printf("Error opening %s\n", devname);
      continue;
    }

    zx_handle_t svc;
    zx_status_t status = fdio_get_service_handle(fd, &svc);
    if (status != ZX_OK) {
      close(fd);
      return status;
    }

    struct device_node* node = (struct device_node*)malloc(sizeof(struct device_node));
    if (!node)
      return -1;

    FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
    int ret = fuchsia_hardware_usb_device_DeviceGetDeviceId(svc, &node->device_id);

    FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
    if (ret != ZX_OK) {
      printf("DeviceGetDeviceId failed for %s\n", devname);
      free(node);
      zx_handle_close(svc);
      close(fd);
      continue;
    }

    FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
    ret = fuchsia_hardware_usb_device_DeviceGetHubDeviceId(svc, &node->hub_id);

    FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
    if (ret != ZX_OK) {
      printf("DeviceGetHubDeviceId failed for %s\n", devname);
      free(node);
      zx_handle_close(svc);
      close(fd);
      continue;
    }
    node->fd = fd;
    node->svc = svc;
    node->depth = -1;
    strlcpy(node->devname, de->d_name, sizeof(node->devname));
    if (devices == NULL) {
      devices = node;
    } else {
      tail->next = node;
    }
    tail = node;
    node->next = NULL;
  }
  closedir(dir);

  int max_depth = 0;
  // compute depths for all device_nodes and compute maximum depth
  struct device_node* node = devices;
  while (node) {
    int depth = get_node_depth(node, devices);
    if (depth > max_depth)
      max_depth = depth;
    node->depth = depth;
    node = node->next;
  }

  // print header
  printf("ID   ");
  for (int i = 0; i < max_depth; i++) {
    printf("    ");
  }
  printf(" VID:PID   SPEED  MANUFACTURER PRODUCT\n");

  // print device tree recursively
  do_list_tree(devices, 0, max_depth);

  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  node = devices;
  while (node) {
    struct device_node* next = node->next;
    zx_handle_close(node->svc);
    close(node->fd);
    free(node);
    node = next;
  }

  return 0;
}

int main(int argc, const char** argv) {
  const char* kTags[] = {"lsusb"};
  int result = 0;
  bool verbose = false;
  bool tree = false;
  const char* device_id = NULL;
  int configuration = -1;
  fx_logger_config_t config = {
      .min_severity = FX_LOG_SEVERITY_DEFAULT,
      .console_fd = -1,
      .log_service_channel = ZX_HANDLE_INVALID,
      .tags = kTags,
      .num_tags = 1,
  };
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if (!strcmp(arg, "-v")) {
      verbose = true;
    } else if (!strcmp(arg, "-t")) {
      tree = true;
    } else if (!strcmp(arg, "-c")) {
      if (++i == argc) {
        printf("configuration required after -c option\n");
        result = -1;
        goto usage;
      }
      configuration = atoi(argv[i]);
    } else if (!strcmp(arg, "-d")) {
      if (++i == argc) {
        printf("device ID required after -d option\n");
        result = -1;
        goto usage;
      }
      device_id = argv[i];

    } else if (!strcmp(arg, "-debug")) {
      config.min_severity = FX_LOG_DEBUG;
    } else {
      printf("unknown option \"%s\"\n", arg);
      result = -1;
      goto usage;
    }
  }
  fx_log_reconfigure(&config);
  FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
  if (tree) {
    FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
    return list_tree();
  } else {
    printf("ID    VID:PID   SPEED  MANUFACTURER PRODUCT\n");
    if (device_id) {
      FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
      return list_device(device_id, configuration, verbose);
    } else {
      FX_LOGF(DEBUG, "", "%s:%i\n", __FILE__, __LINE__);
      return list_devices(verbose);
    }
  }

usage:
  printf("Usage:\n");
  printf("%s [-c <configuration>] [-d <device ID>] [-t] [-v] [-debug]\n", argv[0]);
  printf(
      "  -c   Prints configuration descriptor for specified configuration (rather than current "
      "configuration)\n");
  printf("  -d   Prints only specified device\n");
  printf("  -t   Prints USB device tree\n");
  printf("  -v   Verbose output (prints descriptors)\n");
  return result;
}
