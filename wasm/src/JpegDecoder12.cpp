#include "JpegDecoder12.h"
#include "Exception.h"
#include "Message.h"

#include <setjmp.h>

#include <jerror12.h>
#include <jpeglib12.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace std;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void JpegInitSource12(j_decompress_ptr cinfo) {}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
boolean JpegFillInputBuffer12(j_decompress_ptr cinfo) {
  static uint8_t buf[4] = {0xff, 0xd9, 0, 0};
  cinfo->src->next_input_byte = buf;
  cinfo->src->bytes_in_buffer = 2;

  return TRUE;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void JpegSkipInputData12(j_decompress_ptr cinfo, long nBytes) {
  auto &src = *cinfo->src;
  if (nBytes > 0) {
    while (nBytes > static_cast<long>(src.bytes_in_buffer)) {
      nBytes -= static_cast<long>(src.bytes_in_buffer);
      (*src.fill_input_buffer)(cinfo);
    }
    src.next_input_byte += nBytes;
    src.bytes_in_buffer -= nBytes;
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void JpegTermSource12(j_decompress_ptr cinfo) {}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void ErrorExit12(j_common_ptr cinfo) {
  char buf[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message)(cinfo, buf);
  ThrowNativePixelDecoderException("JpegDecoder12::ErrorExit12::" +
                                   string(buf));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void OutputMessage12(j_common_ptr cinfo) {
  char buf[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message)(cinfo, buf);
  OutputNativePixelDecoderMessage("JpegDecoder12::OutputMessage12::" +
                                  string(buf));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void EmitMessage12(j_common_ptr cinfo, int messageLevel) {
  char buf[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message)(cinfo, buf);
  OutputNativePixelDecoderMessage("JpegDecoder12::EmitMessage12::" +
                                  string(buf));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void DecodeJpeg12(DecoderContext *ctx) {
  jpeg_error_mgr jerr;

  jpeg_decompress_struct cinfo;
  cinfo.err = jpeg_std_error(&jerr);
  cinfo.err->error_exit = ErrorExit12;
  cinfo.err->output_message = OutputMessage12;
  cinfo.err->emit_message = EmitMessage12;
  jpeg_create_decompress(&cinfo);

  jpeg_source_mgr src;
  memset(&src, 0, sizeof(src));

  src.init_source = JpegInitSource12;
  src.fill_input_buffer = JpegFillInputBuffer12;
  src.skip_input_data = JpegSkipInputData12;
  src.resync_to_restart = jpeg_resync_to_restart;
  src.term_source = JpegTermSource12;
  src.bytes_in_buffer = ctx->EncodedBuffer.GetSize();
  src.next_input_byte = ctx->EncodedBuffer.GetData();
  cinfo.src = &src;

  jpeg_read_header(&cinfo, TRUE);

  auto const bytesAllocated =
      (ctx->BitsAllocated / 8) + ((ctx->BitsAllocated % 8 == 0) ? 0 : 1);
  auto const decodedBufferSize = cinfo.image_width * cinfo.image_height *
                                 bytesAllocated * cinfo.num_components;
  ctx->DecodedBuffer.Reset(decodedBufferSize);

  jpeg_start_decompress(&cinfo);

  vector<JSAMPLE *> rows;
  auto const scanlineBytes =
      cinfo.image_width * bytesAllocated * cinfo.num_components;
  auto pDecodedBuffer = ctx->DecodedBuffer.GetData();
  while (cinfo.output_scanline < cinfo.output_height) {
    auto const height = min<size_t>(cinfo.output_height - cinfo.output_scanline,
                                    cinfo.rec_outbuf_height);
    rows.resize(height);
    auto ptr = pDecodedBuffer;
    for (auto i = 0u; i < height; ++i, ptr += scanlineBytes) {
      rows[i] = reinterpret_cast<JSAMPLE *>(ptr);
    }
    auto const n =
        jpeg_read_scanlines(&cinfo, &rows[0], cinfo.rec_outbuf_height);
    pDecodedBuffer += scanlineBytes * n;
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
}
