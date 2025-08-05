idf.py build;
curl -X POST -H "badgehub-api-token: 195daa5cdf961b4a24dea85ec6f773da" \
-F "file=@./build/app_elfs/hello.elf" \
https://badge.why2025.org/api/v3/projects/badgehub_dev/draft/files/hello.elf
echo "did upload"

curl -X 'PATCH' -H "badgehub-api-token: 195daa5cdf961b4a24dea85ec6f773da" \
  'https://badge.why2025.org/api/v3/projects/badgehub_dev/publish' \
  -H 'accept: application/json' \
  -H 'Content-Type: application/json'

echo "did publish"