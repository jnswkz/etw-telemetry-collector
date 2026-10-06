# ETW Telemetry Collector — Cài đặt & vận hành

## Yêu cầu
- Windows 10/11 x64, tài khoản Administrator. Không cần VC++ Redist hay DLL nào khác.
- **Smart App Control** phải tắt (exe chưa ký sẽ bị chặn). Khuyến nghị cài trên VM thử nghiệm.

## Cài đặt
1. Giải nén `etwcollector-<version>-win-x64.zip`.
2. Gỡ cờ "tải từ Internet" (nếu copy qua trình duyệt/mạng):
   `Get-ChildItem <thư mục giải nén> -Recurse | Unblock-File`
3. Chuột phải `install.cmd` → **Run as administrator**
   (hoặc trong PowerShell admin: `.\install-service.ps1`).

Tùy chọn của `install-service.ps1`: `-InstallDir <path>`, `-ForceConfig` (ghi đè config khi nâng cấp), `-NoStart`.
Chạy lại `install.cmd` trên máy đã cài = **nâng cấp** (giữ config và dữ liệu).

| Thành phần | Vị trí |
|---|---|
| Binary + script gỡ | `C:\Program Files\EtwCollector\` |
| Cấu hình | `C:\Program Files\EtwCollector\config\collector.json` |
| Database (SQLite WAL) | `C:\ProgramData\EtwCollector\data\telemetry.sqlite` |
| Log (có rotation) | `C:\ProgramData\EtwCollector\logs\collector.log` |

Thư mục `ProgramData\EtwCollector` chỉ SYSTEM và Administrators truy cập được (command line có thể chứa dữ liệu nhạy cảm).
Đường dẫn tương đối trong `collector.json` được tính dưới `%ProgramData%\EtwCollector`.

Service: tên `EtwTelemetryCollector`, chạy LocalSystem, tự khởi động cùng Windows,
tự khởi động lại sau 5 s nếu crash (2 lần/ngày).

## Kiểm tra sau cài
```powershell
Get-Service EtwTelemetryCollector                       # Running
logman query EtwTelemetryCollector -ets                 # ETW session đang chạy
Get-Content C:\ProgramData\EtwCollector\logs\collector.log -Tail 20
Get-EventLog Application -Source EtwTelemetryCollector -Newest 5
```
Truy vấn DB (cần `sqlite3.exe`, hoặc DB Browser for SQLite):
```sql
SELECT kind, COUNT(*) FROM events GROUP BY kind;
SELECT ts, process_name, parent_name, command_line FROM events WHERE kind = 1 ORDER BY rowid DESC LIMIT 20;
```
`kind`: 1 ProcessCreate, 2 ProcessTerminate, 3–6 File Read/Write/Delete/Rename,
7–9 Reg SetValue/CreateKey/DeleteKey, 10 NetConnect, 11 DnsQuery.

## Chạy tay để debug
Dừng service trước (cùng tên ETW session), rồi trong PowerShell admin:
```powershell
Stop-Service EtwTelemetryCollector
& "C:\Program Files\EtwCollector\etwcollector.exe" --console    # Ctrl+C để dừng
& "C:\Program Files\EtwCollector\etwcollector.exe" --selftest   # không cần ETW
```

## Xuất dữ liệu ra khỏi máy
Không copy trực tiếp `telemetry.sqlite` khi service đang chạy (WAL chưa checkpoint). Chọn một trong hai:
```powershell
Stop-Service EtwTelemetryCollector; Copy-Item C:\ProgramData\EtwCollector\data\telemetry.sqlite* D:\export\; Start-Service EtwTelemetryCollector
```
hoặc (không dừng service): `sqlite3 C:\ProgramData\EtwCollector\data\telemetry.sqlite "VACUUM INTO 'D:\export\telemetry.sqlite'"`

## Gỡ cài đặt
`C:\Program Files\EtwCollector\uninstall-service.ps1` (PowerShell admin) hoặc `uninstall.cmd` trong gói.
Mặc định **giữ** dữ liệu; thêm `-RemoveData` để xóa luôn `%ProgramData%\EtwCollector`.
