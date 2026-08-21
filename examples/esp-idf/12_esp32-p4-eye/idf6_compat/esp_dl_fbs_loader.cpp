#include "fbs_loader.hpp"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"

#include <cinttypes>
#include <climits>
#include <limits>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "psa/crypto.h"
#else
#include "mbedtls/aes.h"
#endif

static const char *TAG = "FbsLoader";

namespace fbs {

namespace {

constexpr uint32_t MAX_PACKED_MODELS = 256;
constexpr uint32_t MAX_MODEL_NAME_LENGTH = 4096;
constexpr size_t UNKNOWN_SOURCE_SIZE = std::numeric_limits<size_t>::max();

struct partition_mapping_t {
    esp_partition_mmap_handle_t handle;
    size_t size;
};

bool is_range_valid(size_t offset, size_t length, size_t total_size)
{
    return offset <= total_size && length <= total_size - offset;
}

bool read_memory_u32(const char *buffer, size_t buffer_size, size_t offset, uint32_t &value)
{
    if (buffer == nullptr || !is_range_valid(offset, sizeof(value), buffer_size)) {
        return false;
    }
    memcpy(&value, buffer + offset, sizeof(value));
    return true;
}

bool read_file_exact(FILE *file, size_t offset, void *buffer, size_t size)
{
    if (file == nullptr || buffer == nullptr || offset > static_cast<size_t>(LONG_MAX)) {
        return false;
    }
    return fseek(file, static_cast<long>(offset), SEEK_SET) == 0 && fread(buffer, 1, size, file) == size;
}

bool get_file_size(FILE *file, size_t &size)
{
    if (file == nullptr || fseek(file, 0, SEEK_END) != 0) {
        return false;
    }
    long end = ftell(file);
    if (end < 0) {
        return false;
    }
    size = static_cast<size_t>(end);
    return true;
}

size_t get_source_size(model_location_type_t location, const void *mapping, const void *fbs_buf)
{
    if (location == MODEL_LOCATION_IN_FLASH_PARTITION && mapping != nullptr) {
        return static_cast<const partition_mapping_t *>(mapping)->size;
    }
    if (location == MODEL_LOCATION_IN_SDCARD && fbs_buf != nullptr) {
        FILE *file = fopen(static_cast<const char *>(fbs_buf), "rb");
        size_t size = 0;
        if (file != nullptr) {
            if (!get_file_size(file, size)) {
                size = 0;
            }
            fclose(file);
        }
        return size;
    }
    // Read-only data embedded by the linker has no length in the public loader API.
    return UNKNOWN_SOURCE_SIZE;
}

bool validate_packed_header(uint32_t model_num, size_t source_size)
{
    if (model_num == 0 || model_num > MAX_PACKED_MODELS) {
        ESP_LOGE(TAG, "Invalid packed model count: %" PRIu32, model_num);
        return false;
    }
    const size_t header_size = 2 * sizeof(uint32_t) + static_cast<size_t>(model_num) * 3 * sizeof(uint32_t);
    if (!is_range_valid(0, header_size, source_size)) {
        ESP_LOGE(TAG, "Packed model header exceeds the source size.");
        return false;
    }
    return true;
}

} // namespace

/**
 * @brief This function is used to decrypt the AES 128-bit CTR mode encrypted data.
 * AES (Advanced Encryption Standard) is a widely-used symmetric encryption algorithm that provides strong security for
 * data protection CTR mode converts the block cipher into a stream cipher, allowing it to encrypt data of any length
 * without the need for padding
 *
 * @param ciphertext   Input Fbs data encrypted by AES 128-bit CTR mode
 * @param plaintext    Decrypted data
 * @param size         Size of input data
 * @param key          128-bit AES key
 */
esp_err_t fbs_aes_crypt_ctr(const uint8_t *ciphertext, uint8_t *plaintext, size_t size, const uint8_t *key)
{
    if (ciphertext == nullptr || plaintext == nullptr || key == nullptr || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t nonce[16] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = PSA_KEY_ID_NULL;
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
    size_t output_length = 0;
    size_t finish_len = 0;
    uint8_t finish_buf[16];
    bool operation_started = false;
    esp_err_t result = ESP_FAIL;
    psa_status_t status;

    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&key_attributes, PSA_ALG_CTR);
    psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&key_attributes, 128);
    status = psa_import_key(&key_attributes, key, 16, &key_id);
    psa_reset_key_attributes(&key_attributes);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to import the model AES key: %ld", static_cast<long>(status));
        return ESP_FAIL;
    }

    status = psa_cipher_encrypt_setup(&operation, key_id, PSA_ALG_CTR);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to set up AES-CTR: %ld", static_cast<long>(status));
        goto cleanup;
    }
    operation_started = true;
    status = psa_cipher_set_iv(&operation, nonce, sizeof(nonce));
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to set the AES-CTR IV: %ld", static_cast<long>(status));
        goto cleanup;
    }
    status = psa_cipher_update(&operation, ciphertext, size, plaintext, size, &output_length);
    if (status != PSA_SUCCESS || output_length != size) {
        ESP_LOGE(TAG,
                 "Failed to decrypt the model with AES-CTR: status=%ld, output=%u, expected=%u",
                 static_cast<long>(status),
                 static_cast<unsigned>(output_length),
                 static_cast<unsigned>(size));
        goto cleanup;
    }

    status = psa_cipher_finish(&operation, finish_buf, sizeof(finish_buf), &finish_len);
    if (status != PSA_SUCCESS || finish_len != 0) {
        ESP_LOGE(TAG,
                 "Failed to finish AES-CTR: status=%ld, trailing_output=%u",
                 static_cast<long>(status),
                 static_cast<unsigned>(finish_len));
        goto cleanup;
    }
    operation_started = false;
    result = ESP_OK;

cleanup:
    if (operation_started) {
        psa_cipher_abort(&operation);
    }
    status = psa_destroy_key(key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to destroy the model AES key: %ld", static_cast<long>(status));
        result = ESP_FAIL;
    }
    return result;
#else
    mbedtls_aes_context aes_ctx;
    size_t offset = 0;
    uint8_t stream_block[16] = {};
    mbedtls_aes_init(&aes_ctx);
    int result = mbedtls_aes_setkey_enc(&aes_ctx, key, 128); // 128-bit key
    if (result == 0) {
        result = mbedtls_aes_crypt_ctr(&aes_ctx, size, &offset, nonce, stream_block, ciphertext, plaintext);
    }
    mbedtls_aes_free(&aes_ctx);
    if (result != 0) {
        ESP_LOGE(TAG, "Failed to decrypt the model with AES-CTR: %d", result);
        return ESP_FAIL;
    }
    return ESP_OK;
#endif
}

/**
    FBS_FILE_FORMAT_EDL1:
    {
        char[4]: "EDL1",
        uint32:  the mode of entru
        uint32:  the length of data
        uint8[]:  the data
    }

    FBS_FILE_FORMAT_PDL1:
    {
        "PDL1": char[4]
        model_num: uint32
        model1_data_offset: uint32
        model1_name_offset: uint32
        model1_name_length: uint32
        model2_data_offset: uint32
        model2_name_offset: uint32
        model2_name_length: uint32
        ...
        model1_name,
        model2_name,
        ...
        model1_data(format:FBS_FILE_FORMAT_EDL1),
        model2_data(format:FBS_FILE_FORMAT_EDL1),
        ...
    }

    FBS_FILE_FORMAT_EDL2:
    {
        char[4]: "EDL2",
        uint32:  the mode of entru
        uint32:  the length of data
        uint32:  zero padding
        uint8[]:  the data
        zero padding
    }

    FBS_FILE_FORMAT_PDL2:
    {
        "PDL2": char[4]
        model_num: uint32
        model1_data_offset: uint32
        model1_name_offset: uint32
        model1_name_length: uint32
        model2_data_offset: uint32
        model2_name_offset: uint32
        model2_name_length: uint32
        ...
        model1_name,
        model2_name,
        ...
        zero padding
        model1_data(format:FBS_FILE_FORMAT_EDL2),
        model2_data(format:FBS_FILE_FORMAT_EDL2),
        ...
    }
*/
typedef enum {
    FBS_FILE_FORMAT_UNK = 0,
    FBS_FILE_FORMAT_EDL1 = 1,
    FBS_FILE_FORMAT_PDL1 = 2,
    FBS_FILE_FORMAT_EDL2 = 3,
    FBS_FILE_FORMAT_PDL2 = 4
} fbs_file_format_t;

fbs_file_format_t get_model_format(const char *fbs_buf,
                                   model_location_type_t model_location,
                                   size_t source_size)
{
    char str[5] = {};
    if (model_location != MODEL_LOCATION_IN_SDCARD) {
        if (!is_range_valid(0, 4, source_size)) {
            ESP_LOGE(TAG, "The model source is shorter than its format header.");
            return FBS_FILE_FORMAT_UNK;
        }
        memcpy(str, fbs_buf, 4);
    } else {
        FILE *f = fopen(fbs_buf, "rb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open %s.", fbs_buf);
            return FBS_FILE_FORMAT_UNK;
        }
        bool read_ok = read_file_exact(f, 0, str, 4);
        fclose(f);
        if (!read_ok) {
            ESP_LOGE(TAG, "Failed to read the model format from %s.", fbs_buf);
            return FBS_FILE_FORMAT_UNK;
        }
    }

    if (strcmp(str, "EDL1") == 0) {
        return FBS_FILE_FORMAT_EDL1;
    } else if (strcmp(str, "PDL1") == 0) {
        return FBS_FILE_FORMAT_PDL1;
    } else if (strcmp(str, "EDL2") == 0) {
        return FBS_FILE_FORMAT_EDL2;
    } else if (strcmp(str, "PDL2") == 0) {
        return FBS_FILE_FORMAT_PDL2;
    } else {
        return FBS_FILE_FORMAT_UNK;
    }
}

esp_err_t get_model_offset_by_index(const char *fbs_buf,
                                    model_location_type_t model_location,
                                    uint32_t index,
                                    uint32_t &offset,
                                    size_t source_size)
{
    uint32_t model_num = 0;
    if (model_location != MODEL_LOCATION_IN_SDCARD) {
        if (!read_memory_u32(fbs_buf, source_size, sizeof(uint32_t), model_num) ||
            !validate_packed_header(model_num, source_size)) {
            return ESP_FAIL;
        }
    } else {
        FILE *f = fopen(fbs_buf, "rb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open %s.", fbs_buf);
            return ESP_FAIL;
        }
        if (!read_file_exact(f, sizeof(uint32_t), &model_num, sizeof(model_num)) ||
            !validate_packed_header(model_num, source_size)) {
            fclose(f);
            return ESP_FAIL;
        }
        if (index >= model_num) {
            ESP_LOGE(TAG, "The model index is out of range.");
            fclose(f);
            return ESP_FAIL;
        }
        const size_t table_offset = (2 + static_cast<size_t>(index) * 3) * sizeof(uint32_t);
        bool read_ok = read_file_exact(f, table_offset, &offset, sizeof(offset));
        fclose(f);
        return read_ok ? ESP_OK : ESP_FAIL;
    }

    if (index >= model_num) {
        ESP_LOGE(TAG, "The model index is out of range.");
        return ESP_FAIL;
    }
    const size_t table_offset = (2 + static_cast<size_t>(index) * 3) * sizeof(uint32_t);
    return read_memory_u32(fbs_buf, source_size, table_offset, offset) ? ESP_OK : ESP_FAIL;
}

esp_err_t get_model_offset_by_name(const char *fbs_buf,
                                   model_location_type_t model_location,
                                   const char *name,
                                   uint32_t &offset,
                                   size_t source_size)
{
    if (name == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (model_location != MODEL_LOCATION_IN_SDCARD) {
        uint32_t model_num = 0;
        if (!read_memory_u32(fbs_buf, source_size, sizeof(uint32_t), model_num) ||
            !validate_packed_header(model_num, source_size)) {
            return ESP_FAIL;
        }
        for (uint32_t i = 0; i < model_num; i++) {
            uint32_t name_offset = 0;
            uint32_t name_length = 0;
            const size_t table_offset = (2 + static_cast<size_t>(i) * 3) * sizeof(uint32_t);
            if (!read_memory_u32(fbs_buf, source_size, table_offset + sizeof(uint32_t), name_offset) ||
                !read_memory_u32(fbs_buf, source_size, table_offset + 2 * sizeof(uint32_t), name_length) ||
                name_length == 0 || name_length > MAX_MODEL_NAME_LENGTH ||
                !is_range_valid(name_offset, name_length, source_size)) {
                ESP_LOGE(TAG, "Invalid packed model name metadata at index %" PRIu32 ".", i);
                return ESP_FAIL;
            }
            std::string model_name(fbs_buf + name_offset, name_length);
            if (model_name == std::string(name)) {
                return read_memory_u32(fbs_buf, source_size, table_offset, offset) ? ESP_OK : ESP_FAIL;
            }
        }
        ESP_LOGE(TAG, "Model %s is not found.", name);
        return ESP_FAIL;
    } else {
        FILE *f = fopen(fbs_buf, "rb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open %s.", fbs_buf);
            return ESP_FAIL;
        }
        uint32_t model_num = 0;
        if (!read_file_exact(f, sizeof(uint32_t), &model_num, sizeof(model_num)) ||
            !validate_packed_header(model_num, source_size)) {
            fclose(f);
            return ESP_FAIL;
        }
        for (uint32_t i = 0; i < model_num; i++) {
            uint32_t name_offset = 0;
            uint32_t name_length = 0;
            const size_t table_offset = (2 + static_cast<size_t>(i) * 3) * sizeof(uint32_t);
            if (!read_file_exact(f, table_offset + sizeof(uint32_t), &name_offset, sizeof(name_offset)) ||
                !read_file_exact(f, table_offset + 2 * sizeof(uint32_t), &name_length, sizeof(name_length)) ||
                name_length == 0 || name_length > MAX_MODEL_NAME_LENGTH ||
                !is_range_valid(name_offset, name_length, source_size)) {
                ESP_LOGE(TAG, "Invalid packed model name metadata at index %" PRIu32 ".", i);
                fclose(f);
                return ESP_FAIL;
            }
            std::string model_name(name_length, '\0');
            if (!read_file_exact(f, name_offset, model_name.data(), name_length)) {
                fclose(f);
                return ESP_FAIL;
            }
            if (model_name == std::string(name)) {
                bool read_ok = read_file_exact(f, table_offset, &offset, sizeof(offset));
                fclose(f);
                return read_ok ? ESP_OK : ESP_FAIL;
            }
        }
        ESP_LOGE(TAG, "Model %s is not found.", name);
        fclose(f);
        return ESP_FAIL;
    }
}

FbsModel *create_fbs_model(const char *fbs_buf,
                           fbs_file_format_t format,
                           model_location_type_t model_location,
                           uint32_t offset,
                           const uint8_t *key,
                           bool param_copy,
                           size_t source_size)
{
    if (fbs_buf == nullptr) {
        ESP_LOGE(TAG, "Model's flatbuffers is empty.");
        return nullptr;
    }

    const size_t model_header_size =
        (format == FBS_FILE_FORMAT_EDL1 || format == FBS_FILE_FORMAT_PDL1) ? 12 : 16;
    char *model_buf = nullptr;
    uint32_t mode = 0;
    uint32_t size = 0;
    if (model_location != MODEL_LOCATION_IN_SDCARD) {
        if (!is_range_valid(offset, model_header_size, source_size) ||
            !read_memory_u32(fbs_buf, source_size, static_cast<size_t>(offset) + sizeof(uint32_t), mode) ||
            !read_memory_u32(fbs_buf, source_size, static_cast<size_t>(offset) + 2 * sizeof(uint32_t), size) ||
            size == 0 || !is_range_valid(static_cast<size_t>(offset) + model_header_size, size, source_size)) {
            ESP_LOGE(TAG, "The model header or data range is invalid.");
            return nullptr;
        }
        model_buf = const_cast<char *>(fbs_buf + static_cast<size_t>(offset) + model_header_size);
    } else {
        FILE *f = fopen(fbs_buf, "rb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open %s.", fbs_buf);
            return nullptr;
        }
        if (!is_range_valid(offset, model_header_size, source_size) ||
            !read_file_exact(f, static_cast<size_t>(offset) + sizeof(uint32_t), &mode, sizeof(mode)) ||
            !read_file_exact(f, static_cast<size_t>(offset) + 2 * sizeof(uint32_t), &size, sizeof(size)) || size == 0 ||
            !is_range_valid(static_cast<size_t>(offset) + model_header_size, size, source_size)) {
            ESP_LOGE(TAG, "Failed to read model header from %s.", fbs_buf);
            fclose(f);
            return nullptr;
        }
        model_buf = (char *)dl::tool::malloc_aligned(16, size, MALLOC_CAP_DEFAULT);
        if (!model_buf) {
            ESP_LOGE(
                TAG,
                "Failed to alloc %.2fKB RAM, largest available PSRAM block size %.2fKB, internal RAM block size %.2fKB",
                size / 1024.f,
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024.f,
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024.f);
            fclose(f);
            return nullptr;
        }
        if (!read_file_exact(f, static_cast<size_t>(offset) + model_header_size, model_buf, size)) {
            ESP_LOGE(TAG, "Failed to read model data from %s.", fbs_buf);
            heap_caps_free(model_buf);
            fclose(f);
            return nullptr;
        }
        fclose(f);
    }

    if (mode != 0 && mode != 1) {
        ESP_LOGE(TAG, "Unsupported model encryption mode: %" PRIu32, mode);
        if (model_location == MODEL_LOCATION_IN_SDCARD) {
            heap_caps_free(model_buf);
        }
        return nullptr;
    }
    if (mode != 0 && key == NULL) {
        ESP_LOGE(TAG, "This is a cryptographic model, please enter the secret key!");
        if (model_location == MODEL_LOCATION_IN_SDCARD) {
            heap_caps_free(model_buf);
        }
        return nullptr;
    }

    bool rodata_move = false;
    if (model_location == MODEL_LOCATION_IN_FLASH_RODATA &&
        dl::tool::memory_addr_type(model_buf) == dl::MEMORY_ADDR_PSRAM) {
        ESP_LOGW(TAG,
                 "CONFIG_SPIRAM_RODATA or CONFIG_SPIRAM_XIP_FROM_PSRAM option is on, fbs model is copyed to PSRAM.");
        rodata_move = true;
    }

    bool auto_free;
    if (mode == 0) { // without encryption
        auto_free = (model_location == MODEL_LOCATION_IN_SDCARD) ? true : false;
        bool address_align = !(reinterpret_cast<uintptr_t>(model_buf) & 0xf);
        if (format == FBS_FILE_FORMAT_EDL1 || format == FBS_FILE_FORMAT_PDL1) {
            param_copy = true;
        } else if (!address_align) {
            ESP_LOGW(TAG, "The address of fbs model in flash is not aligned with 16 bytes.");
            param_copy = true;
        } else {
            if (model_location == MODEL_LOCATION_IN_SDCARD) {
                param_copy = false;
            } else if (dl::tool::memory_addr_type(model_buf) == dl::MEMORY_ADDR_PSRAM) {
                param_copy = false;
            }
        }
    } else { // 128-bit AES encryption
        auto_free = true;
        param_copy = (format == FBS_FILE_FORMAT_EDL1 || format == FBS_FILE_FORMAT_PDL1) ? true : false;
        uint8_t *model_buf_decrypt;
        if (model_location == MODEL_LOCATION_IN_SDCARD) {
            model_buf_decrypt = (uint8_t *)model_buf;
        } else {
            model_buf_decrypt = (uint8_t *)dl::tool::malloc_aligned(16, size, MALLOC_CAP_DEFAULT);
            if (!model_buf_decrypt) {
                ESP_LOGE(TAG,
                         "Failed to alloc %.2fKB RAM, largest available PSRAM block size %.2fKB, internal RAM block "
                         "size %.2fKB",
                         size / 1024.f,
                         heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024.f,
                         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024.f);
                return nullptr;
            }
        }
        if (fbs_aes_crypt_ctr((const uint8_t *)model_buf, model_buf_decrypt, size, key) != ESP_OK) {
            heap_caps_free(model_buf_decrypt);
            return nullptr;
        }
        model_buf = (char *)model_buf_decrypt;
    }

    return new FbsModel(model_buf, size, model_location, mode, rodata_move, auto_free, param_copy);
}

FbsLoader::FbsLoader(const char *name, model_location_type_t location) :
    m_mmap_handle(nullptr), m_location(location), m_fbs_buf(nullptr)
{
    if (name == nullptr) {
        return;
    }

    if (m_location == MODEL_LOCATION_IN_FLASH_RODATA || m_location == MODEL_LOCATION_IN_SDCARD) {
        m_fbs_buf = (const void *)name;
    } else if (m_location == MODEL_LOCATION_IN_FLASH_PARTITION) {
        const esp_partition_t *partition =
            esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, name);
        if (partition) {
            int free_pages = spi_flash_mmap_get_free_pages(SPI_FLASH_MMAP_DATA);
            if (free_pages <= 0) {
                ESP_LOGE(TAG, "No flash MMU pages are available for partition %s", partition->label);
                return;
            }
            size_t storage_size = static_cast<size_t>(free_pages) * 64 * 1024; // Byte
            ESP_LOGI(TAG, "The storage free size is %u KB", static_cast<unsigned>(storage_size / 1024));
            ESP_LOGI(TAG,
                     "The partition size is %" PRIu32 " KB",
                     static_cast<uint32_t>(partition->size / 1024));
            if (storage_size < partition->size) {
                ESP_LOGE(TAG,
                         "The storage free size of this board is less than %s partition required size",
                         partition->label);
                return;
            }
            partition_mapping_t *mapping = static_cast<partition_mapping_t *>(malloc(sizeof(partition_mapping_t)));
            if (mapping == nullptr) {
                ESP_LOGE(TAG, "Failed to allocate the partition mapping state.");
                return;
            }
            esp_err_t err = esp_partition_mmap(partition,
                                               0,
                                               partition->size,
                                               ESP_PARTITION_MMAP_DATA,
                                               &this->m_fbs_buf,
                                               &mapping->handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to map partition %s: %s", partition->label, esp_err_to_name(err));
                free(mapping);
                this->m_fbs_buf = nullptr;
                return;
            }
            mapping->size = partition->size;
            this->m_mmap_handle = mapping;
        } else {
            ESP_LOGE(TAG, "Can not find %s in partition table", name);
        }
    }
}

FbsLoader::~FbsLoader()
{
    if (m_location == MODEL_LOCATION_IN_FLASH_PARTITION && this->m_mmap_handle) {
        partition_mapping_t *mapping = static_cast<partition_mapping_t *>(this->m_mmap_handle);
        esp_partition_munmap(mapping->handle); // support esp-idf v5
        free(this->m_mmap_handle);
        this->m_mmap_handle = nullptr;
    }
}

FbsModel *FbsLoader::load(const int model_index, const uint8_t *key, bool param_copy)
{
    if (this->m_fbs_buf == nullptr) {
        ESP_LOGE(TAG, "Model's flatbuffers is empty.");
        return nullptr;
    }

    uint32_t offset = 0;
    const size_t source_size = get_source_size(m_location, m_mmap_handle, m_fbs_buf);
    if (source_size == 0) {
        ESP_LOGE(TAG, "The model source is empty or unavailable.");
        return nullptr;
    }
    fbs_file_format_t format = get_model_format((const char *)m_fbs_buf, m_location, source_size);
    if (format == FBS_FILE_FORMAT_PDL1 || format == FBS_FILE_FORMAT_PDL2) {
        // packed multiple espdl models
        if (model_index < 0 ||
            get_model_offset_by_index((const char *)m_fbs_buf, m_location, model_index, offset, source_size) != ESP_OK) {
            return nullptr;
        }
    } else if (format == FBS_FILE_FORMAT_EDL1 || format == FBS_FILE_FORMAT_EDL2) {
        // single espdl model
        if (model_index > 0) {
            ESP_LOGW(TAG, "There is only one model in the flatbuffers, ignore the input model index!");
        }
        offset = 0;
    } else {
        ESP_LOGE(TAG, "Unsupported format, or the model file is corrupted!");
        return nullptr;
    }
    return create_fbs_model((const char *)m_fbs_buf, format, m_location, offset, key, param_copy, source_size);
}

FbsModel *FbsLoader::load(const uint8_t *key, bool param_copy)
{
    return this->load(0, key, param_copy);
}

FbsModel *FbsLoader::load(const char *model_name, const uint8_t *key, bool param_copy)
{
    if (this->m_fbs_buf == nullptr) {
        ESP_LOGE(TAG, "Model's flatbuffers is empty.");
        return nullptr;
    }

    uint32_t offset = 0;
    const size_t source_size = get_source_size(m_location, m_mmap_handle, m_fbs_buf);
    if (source_size == 0) {
        ESP_LOGE(TAG, "The model source is empty or unavailable.");
        return nullptr;
    }
    fbs_file_format_t format = get_model_format((const char *)m_fbs_buf, m_location, source_size);
    if (format == FBS_FILE_FORMAT_PDL1 || format == FBS_FILE_FORMAT_PDL2) {
        // packed multiple espdl models
        if (get_model_offset_by_name((const char *)m_fbs_buf, m_location, model_name, offset, source_size) != ESP_OK) {
            return nullptr;
        }
    } else if (format == FBS_FILE_FORMAT_EDL1 || format == FBS_FILE_FORMAT_EDL2) {
        // single espdl model
        if (model_name) {
            ESP_LOGW(TAG, "There is only one model in the flatbuffers, ignore the input model name!");
        }
        offset = 0;
    } else {
        ESP_LOGE(TAG, "Unsupported format, or the model file is corrupted!");
        return nullptr;
    }
    return create_fbs_model((const char *)m_fbs_buf, format, m_location, offset, key, param_copy, source_size);
}

int FbsLoader::get_model_num()
{
    if (this->m_fbs_buf == nullptr) {
        return 0;
    }

    const size_t source_size = get_source_size(m_location, m_mmap_handle, m_fbs_buf);
    if (source_size == 0) {
        return 0;
    }
    fbs_file_format_t format = get_model_format((const char *)m_fbs_buf, m_location, source_size);
    if (format == FBS_FILE_FORMAT_PDL1 || format == FBS_FILE_FORMAT_PDL2) {
        // packed multiple espdl models
        uint32_t model_num = 0;
        if (m_location != MODEL_LOCATION_IN_SDCARD) {
            if (!read_memory_u32((const char *)m_fbs_buf, source_size, sizeof(uint32_t), model_num)) {
                return 0;
            }
        } else {
            FILE *f = fopen((const char *)m_fbs_buf, "rb");
            if (!f) {
                ESP_LOGE(TAG, "Failed to open %s.", (const char *)m_fbs_buf);
                return 0;
            }
            bool read_ok = read_file_exact(f, sizeof(uint32_t), &model_num, sizeof(model_num));
            fclose(f);
            if (!read_ok) {
                ESP_LOGE(TAG, "Failed to read the packed model count from %s.", (const char *)m_fbs_buf);
                return 0;
            }
        }
        if (!validate_packed_header(model_num, source_size)) {
            return 0;
        }
        return model_num;
    } else if (format == FBS_FILE_FORMAT_EDL1 || format == FBS_FILE_FORMAT_EDL2) {
        // single espdl model
        return 1;
    } else {
        ESP_LOGE(TAG, "Unsupported format, or the model file is corrupted!");
        return 0;
    }

    return 0;
}

void FbsLoader::list_models()
{
    if (this->m_fbs_buf == nullptr) {
        ESP_LOGE(TAG, "Model's flatbuffers is empty.");
        return;
    }

    const size_t source_size = get_source_size(m_location, m_mmap_handle, m_fbs_buf);
    if (source_size == 0) {
        ESP_LOGE(TAG, "The model source is empty or unavailable.");
        return;
    }
    fbs_file_format_t format = get_model_format((const char *)m_fbs_buf, m_location, source_size);
    if (format == FBS_FILE_FORMAT_PDL1 || format == FBS_FILE_FORMAT_PDL2) {
        // packed multiple espdl models
        if (m_location != MODEL_LOCATION_IN_SDCARD) {
            uint32_t model_num = 0;
            if (!read_memory_u32((const char *)m_fbs_buf, source_size, sizeof(uint32_t), model_num) ||
                !validate_packed_header(model_num, source_size)) {
                return;
            }
            for (uint32_t i = 0; i < model_num; i++) {
                uint32_t name_offset = 0;
                uint32_t name_length = 0;
                const size_t table_offset = (2 + static_cast<size_t>(i) * 3) * sizeof(uint32_t);
                if (!read_memory_u32((const char *)m_fbs_buf,
                                     source_size,
                                     table_offset + sizeof(uint32_t),
                                     name_offset) ||
                    !read_memory_u32((const char *)m_fbs_buf,
                                     source_size,
                                     table_offset + 2 * sizeof(uint32_t),
                                     name_length) ||
                    name_length == 0 || name_length > MAX_MODEL_NAME_LENGTH ||
                    !is_range_valid(name_offset, name_length, source_size)) {
                    ESP_LOGE(TAG, "Invalid packed model name metadata at index %" PRIu32 ".", i);
                    return;
                }
                std::string name((const char *)m_fbs_buf + name_offset, name_length);
                ESP_LOGI(TAG, "model name: %s, index:%" PRIu32, name.c_str(), i);
            }
        } else {
            FILE *f = fopen((const char *)m_fbs_buf, "rb");
            if (!f) {
                ESP_LOGE(TAG, "Failed to open %s.", (const char *)m_fbs_buf);
                return;
            }
            uint32_t model_num = 0;
            if (!read_file_exact(f, sizeof(uint32_t), &model_num, sizeof(model_num)) ||
                !validate_packed_header(model_num, source_size)) {
                fclose(f);
                return;
            }
            for (uint32_t i = 0; i < model_num; i++) {
                uint32_t name_offset = 0;
                uint32_t name_length = 0;
                const size_t table_offset = (2 + static_cast<size_t>(i) * 3) * sizeof(uint32_t);
                if (!read_file_exact(f, table_offset + sizeof(uint32_t), &name_offset, sizeof(name_offset)) ||
                    !read_file_exact(f, table_offset + 2 * sizeof(uint32_t), &name_length, sizeof(name_length)) ||
                    name_length == 0 || name_length > MAX_MODEL_NAME_LENGTH ||
                    !is_range_valid(name_offset, name_length, source_size)) {
                    ESP_LOGE(TAG, "Invalid packed model name metadata at index %" PRIu32 ".", i);
                    fclose(f);
                    return;
                }
                std::string name(name_length, '\0');
                if (!read_file_exact(f, name_offset, name.data(), name_length)) {
                    ESP_LOGE(TAG, "Failed to read packed model name at index %" PRIu32 ".", i);
                    fclose(f);
                    return;
                }
                ESP_LOGI(TAG, "model name: %s, index:%" PRIu32, name.c_str(), i);
            }
            fclose(f);
        }
    } else if (format == FBS_FILE_FORMAT_EDL1 || format == FBS_FILE_FORMAT_EDL2) {
        ESP_LOGI(TAG, "There is only one model in the flatbuffers without model name.");
    }
}

const char *FbsLoader::get_model_location_string()
{
    switch (m_location) {
    case MODEL_LOCATION_IN_FLASH_RODATA:
        return "MODEL LOCATION IN FLASH RODATA";
    case MODEL_LOCATION_IN_FLASH_PARTITION:
        return "MODEL LOCATION IN FLASH PARTITION";
    case MODEL_LOCATION_IN_SDCARD:
        return "MODEL LOCATION IN SDCARD";
    default:
        return "MODEL LOCATION UNK";
    }
    return "MODEL LOCATION UNK";
}

} // namespace fbs
