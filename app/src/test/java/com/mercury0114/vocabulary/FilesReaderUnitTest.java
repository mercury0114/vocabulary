package com.mercury0114.vocabulary;

import static com.mercury0114.vocabulary.FilesReader.FileNotFolderException;
import static com.mercury0114.vocabulary.FilesReader.GetFilesNames;
import static com.mercury0114.vocabulary.FilesReader.readLinesAndSort;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableList;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class FilesReaderUnitTest {
  @Rule public TemporaryFolder temporaryFolder = new TemporaryFolder();

  @Test
  public void getFilesNames_emptyFolder_returnsEmptyList() throws IOException {
    File emptyFolder = temporaryFolder.newFolder("empty_folder");
    assertEquals(GetFilesNames(emptyFolder).size(), 0);
  }

  @Test
  public void getFilesNames_oneFile_returnsOneName() throws IOException {
    File oneFileFolder = temporaryFolder.newFolder("one_file_folder");
    File file = temporaryFolder.newFile("one_file_folder/file.txt");
    assertEquals(GetFilesNames(oneFileFolder).size(), 1);
  }

  @Test
  public void getFilesNames_notFolder_throwsException() throws IOException {
    File file = temporaryFolder.newFile("file.txt");
    FileNotFolderException exception =
        assertThrows(FileNotFolderException.class, () -> GetFilesNames(file));
    assertEquals("file.txt", exception.getMessage());
  }

  @Test
  public void getFilesNames_returnsSortedList() throws IOException {
    File folder = temporaryFolder.newFolder("folder");
    temporaryFolder.newFile("folder/b");
    temporaryFolder.newFile("folder/c");
    temporaryFolder.newFile("folder/a");
    List<String> names = GetFilesNames(folder);
    assertEquals(names.size(), 3);
    assertEquals(names.get(0), "a");
    assertEquals(names.get(1), "b");
    assertEquals(names.get(2), "c");
  }

  @Test
  public void readLinesAndSort_returnsSortedList() throws IOException {
    File file = temporaryFolder.newFile("file.txt");
    ImmutableList<String> fileContent =
        ImmutableList.of("question1, answer1", "question0, answer0");

    Files.write(Paths.get(file.getPath()), fileContent, StandardCharsets.UTF_8);

    ImmutableList<String> lines = readLinesAndSort(file);
    assertEquals(lines.get(0), "question0, answer0");
    assertEquals(lines.get(1), "question1, answer1");
  }
}
