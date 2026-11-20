# Dữ liệu thực nghiệm chính (mục 3.3)

Bộ dữ liệu gồm 20 ảnh gốc và 60 ảnh mức xám đã tiền xử lý: 20 ảnh cho mỗi kích thước 512×512, 1024×1024 và 2048×2048.

## Nội dung

- `raw/`: 20 ảnh gốc JPG, giữ local và không đưa lên GitHub.
- `processed/<kích_thước>/*.bin`: ảnh mức xám float32 little-endian dùng trực tiếp bởi benchmark; giữ local và không đưa lên GitHub.
- `processed/metadata.csv`: ánh xạ ID ảnh, ảnh nguồn, thông tin crop/kích thước và đường dẫn `.bin`; được giữ trong repository.
- `processed_png/`: ảnh xem trước PNG; giữ local và không đưa lên GitHub.
- `../scripts/data/preprocess.py`: script tái tạo dữ liệu đã xử lý từ ảnh gốc.

## Tạo lại dữ liệu

Đặt đúng 20 ảnh nguồn vào `data/raw/`, cài NumPy và Pillow, rồi chạy từ thư mục gốc dự án:

```bash
python -m pip install numpy Pillow
python scripts/data/preprocess.py --numbered --output data/processed
```

Các ảnh nguồn phải có kích thước tối thiểu 2048×2048. Script ghi `metadata.csv` cùng các tệp `.bin`. Các ảnh dữ liệu bị `.gitignore` loại khỏi Git; metadata và script vẫn được lưu để mô tả và tái tạo thực nghiệm.

Nguồn và tình trạng giấy phép ảnh được ghi tại [SOURCES.md](SOURCES.md); hiện chưa có URL/tên giấy phép được lưu cho các ảnh này.
